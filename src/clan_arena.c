//
// Clan Arena related
//

#include "g_local.h"

#define MAX_TM_STATS (MAX_CLIENTS)

static int round_num;
static int team1_score;
static int team2_score;

qbool is_rules_change_allowed( void );

void SM_PrepareCA(void)
{
	if( !isCA() )
		return;

	team1_score = team2_score = 0;
	round_num = 1;
}

int CA_rounds(void)
{
	return bound(3, cvar("k_clan_arena_rounds"), 101);
}

int CA_wins_required(void)
{
	int k_clan_arena_rounds = CA_rounds();
	k_clan_arena_rounds += (k_clan_arena_rounds % 2) ? 0 : 1;
	return (k_clan_arena_rounds + 1)/2;
}

qbool isCA( )
{
	return ( isTeam() && cvar("k_clan_arena") );
}

// hard coded default settings for CA
static char ca_settings[] =
	"k_clan_arena_rounds 9\n"
	"dp 0\n"
	"teamplay 4\n"
	"deathmatch 5\n"
	"timelimit 30\n"
	"k_overtime 0\n"
	"k_spw 1\n"
	"k_dmgfrags 1\n"
	"k_noitems 1\n"
	"k_exclusive 0\n"
	"k_membercount 1\n";

void apply_CA_settings(void)
{
    char buf[1024*4];
	char *cfg_name;

	if ( !isCA() )
		return;

	trap_readcmd( ca_settings, buf, sizeof(buf) );
	G_cprint("%s", buf);

	cfg_name = va("configs/usermodes/ca/default.cfg");
	if ( can_exec( cfg_name ) )
	{
		trap_readcmd( va("exec %s\n", cfg_name), buf, sizeof(buf) );
		G_cprint("%s", buf);
	}

	cfg_name = va("configs/usermodes/ca/%s.cfg", g_globalvars.mapname);
	if ( can_exec( cfg_name ) )
	{
		trap_readcmd( va("exec %s\n", cfg_name), buf, sizeof(buf) );
		G_cprint("%s", buf);
	}

	G_cprint("\n");
}

void ToggleCArena()
{
	if ( !is_rules_change_allowed() )
		return;

	if ( !isCA() ) {
		// seems we trying turn CA on.
		if ( !isTeam() )
		{
			G_sprint(self, 2, "Set %s mode first\n", redtext("team"));
			return;
		}
	}

	cvar_toggle_msg( self, "k_clan_arena", redtext("Clan Arena") );

	apply_CA_settings();
}

void CA_reset_round_stats(void)
{
    int from1 = 0;
    gedict_t *p;

    for( p = world; (p = find_plrghst( p, &from1 )); )
    {
        p->ps.ca_prernd_dmg_g       =   p->ps.dmg_g;                // damage given
        p->ps.ca_prernd_dmg_g_rl    =   p->ps.dmg_g_rl;             // RL damage given
        p->ps.ca_prernd_frags       =   p->s.v.frags ;              // frags
        p->ps.ca_prernd_rla         =   p->ps.wpn[wpRL].attacks;    // RL attacks
        p->ps.ca_prernd_rlh         =   p->ps.wpn[wpRL].hits;       // RL real hits
        p->ps.ca_prernd_rlv         =   p->ps.wpn[wpRL].rhits;      // RL virtual hits
        p->ps.ca_prernd_lga         =   p->ps.wpn[wpLG].attacks;    // LG attacks
        p->ps.ca_prernd_lgh         =   p->ps.wpn[wpLG].hits;       // LG real hits
    }
}

void CA_print_round_stats(void)
{
    int from1 = 0;
    gedict_t *p;

    G_bprint (PRINT_MEDIUM, "player     score  dmg rh rd skill  lgf lgh    lg%%\n" );
    G_bprint (PRINT_MEDIUM, "---------- -----  --- -- -- -----  --- --- ------\n" );

    for( p = world; (p = find_plrghst( p, &from1 )); )
    {
        float dmg = p->ps.dmg_g - p->ps.ca_prernd_dmg_g;
        float drl = p->ps.dmg_g_rl - p->ps.ca_prernd_dmg_g_rl;
        float frg = p->s.v.frags - p->ps.ca_prernd_frags;
        int rla = p->ps.wpn[wpRL].attacks - p->ps.ca_prernd_rla;
        int rlh = p->ps.wpn[wpRL].hits - p->ps.ca_prernd_rlh;
        int rlv = p->ps.wpn[wpRL].rhits - p->ps.ca_prernd_rlv;
        int lga = p->ps.wpn[wpLG].attacks - p->ps.ca_prernd_lga;
        int lgh = p->ps.wpn[wpLG].hits - p->ps.ca_prernd_lgh;

        G_bprint (PRINT_MEDIUM, "%-10.10s    %2.0f %4.0f %2d %2d %5.1f   %2d  %2d %5.1f%%\n",p->s.v.netname,frg,dmg,rlv,rlh,rlv ? ( drl / rlv ) : 0, lga, lgh, 100.0 * lgh  / max(1, lga));
    }
}

void CA_change_pov(void)
{
    gedict_t *p;

    if ( self->trackent > 0 && self->trackent <= MAX_CLIENTS )
        p = &g_edicts[ self->trackent ];
    else
        p = world;

    for( ; (p = find_plr( p )); )
    {
        if ( ISLIVE( p ) && streq(self->ca_oldteam,p->ca_oldteam ) ) {
            self->trackent = NUM_FOR_EDICT( p ? p : world );
            if ( p ) {
                G_sprint( self, 2, "tracking %s\n", getname( p )) ;
            }
        }
    }
}

void CA_dead_jump_button( void )
{
    if ( !self->s.v.button2 )
    {
        self->s.v.flags = ( ( int ) ( self->s.v.flags ) ) | FL_JUMPRELEASED;
        return;
    }

    if ( !( ( ( int ) ( self->s.v.flags ) ) & FL_JUMPRELEASED ) )
        return;

    self->s.v.flags = (int)self->s.v.flags & ~FL_JUMPRELEASED;

    // switch pov.
    CA_change_pov();
}

void CA_PutClientInServer(void)
{
	if ( !isCA() )
		return;

	// set CA self params
	if ( match_in_progress == 2 )
	{
		int items;

		self->s.v.ammo_nails   = 150;
		self->s.v.ammo_shells  = 200;
		self->s.v.ammo_rockets = 40;
		self->s.v.ammo_cells   = 100;

		self->s.v.armorvalue   = 200;
		self->s.v.armortype    = 0.8;
		self->s.v.health       = 150;
        self->ca_grenades      = 10;

		items = 0;
		items |= IT_AXE;
		items |= IT_SHOTGUN;
		items |= IT_NAILGUN;
		items |= IT_SUPER_NAILGUN;
		items |= IT_SUPER_SHOTGUN;
		items |= IT_ROCKET_LAUNCHER;
		items |= IT_GRENADE_LAUNCHER;
		items |= IT_LIGHTNING;
		items |= IT_ARMOR3; // add red armor

		self->s.v.items = items;

		// { remove invincibility/quad if any
		self->invincible_time = 0;
		self->invincible_finished = 0;
		self->super_time = 0;
		self->super_damage_finished = 0;
		// }

		// default to spawning with rl
		self->s.v.weapon = IT_ROCKET_LAUNCHER;
	}

	// set to ghost if dead
	if ( ISDEAD( self ) )
	{
		self->s.v.solid		 = SOLID_NOT;
		//self->s.v.movetype	 = MOVETYPE_NOCLIP;
        self->s.v.movetype     = MOVETYPE_WALK;
		self->vw_index		 = 0;
		setmodel( self, "" );
        self->s.v.armorvalue   = 0;
        self->s.v.health       = 0;

		setorigin (self, PASSVEC3( self->s.v.origin ) );
	}
}

qbool CA_can_fire( gedict_t *p )
{
	if ( !p )
		return false;

	if ( !isCA() )
		return true;

	return ( ISLIVE( p ) && ra_match_fight == 2 && time_to_start && g_globalvars.time >= time_to_start );
}

// return 0 if there no alive teams
// return 1 if there one alive team and alive_team point to 1 or 2 wich refering to _k_team1 or _k_team2 cvars
// return 2 if there at least two alive teams
static int CA_check_alive_teams( int *alive_team )
{
	gedict_t *p;
	qbool few_alive_teams = false;
	char *first_team = NULL;

	if ( alive_team )
		*alive_team = 0;

	for( p = world; (p = find_plr( p )); )
	{
		if ( !first_team )
		{
			if ( ISLIVE( p ) )
				first_team = getteam( p ); // ok, we found first team with alive players

			continue;
		}

		if ( strneq( first_team, getteam( p ) ) )
		{
			if ( ISLIVE( p ) )
			{
				few_alive_teams = true; // we found at least two teams with alive players
				break;
			}
		}
	}

	if ( few_alive_teams )
		return 2;

	if ( first_team )
	{
		if ( alive_team )
		{
			*alive_team = streq( first_team, cvar_string("_k_team1") ) ? 1 : 2;
		}
		return 1;
	}

	return 0;
}

void CA_damage_live_players( int dodamage )
{
	gedict_t *p;
	for( p = world; (p = find_plr( p )); )
	{
		if ( ISLIVE( p ) )
		{
			p->s.v.armorvalue = p->s.v.armorvalue - (dodamage * 2);

			if ( p->s.v.armorvalue < 0 )
				p->s.v.armorvalue = 0;

			p->s.v.health = p->s.v.health - dodamage;

			if ( p->s.v.health <= 0 ) {
				G_bprint (PRINT_MEDIUM, "%s ran out of health\n", p->s.v.netname);
		                p->s.v.solid		 = SOLID_NOT;
		                p->vw_index		 = 0;
		                setmodel( p, "" );
			}
			else {
				sound( p, CHAN_VOICE, va("player/pain%d.wav", i_rnd(1,6)), 1, ATTN_NORM );
			}
		}
	}
}

void CA_PrintScores(void)
{
	int s1 = team1_score;
	int s2 = team2_score;
	char *t1 = cvar_string( "_k_team1" );
	char *t2 = cvar_string( "_k_team2" );

	G_sprint(self, 2, "%s \x90%s\x91 = %s\n", redtext("Team"),
							 (s1 > s2 ? t1 : t2), dig3(s1 > s2 ? s1 : s2));
	G_sprint(self, 2, "%s \x90%s\x91 = %s\n", redtext("Team"),
							 (s1 > s2 ? t2 : t1), dig3(s1 > s2 ? s2 : s1));
}

void CA_TeamsStats(void)
{
	if (team1_score != team2_score)
	{
		G_bprint( 2, "Team \x90%s\x91 wins %d to %d\n",
			cvar_string(va("_k_team%d", team1_score > team2_score ? 1 : 2)),
			team1_score > team2_score ? team1_score : team2_score,
			team1_score > team2_score ? team2_score : team1_score);
	}
	else
	{
		G_bprint( 2, "%s have equal scores %d\n", redtext("Teams"), team1_score );
	}
}

void CA_SetDeadTeams(void)
{
    gedict_t *p;
    char *deadteam = redtext("dead");

    if(ra_match_fight != 2){
        return;
    }

    for( p = world; (p = find_plr( p )); )
    {
        if( ISDEAD(p) && strneq(getteam(p),deadteam) ) {
            strcpy(p->ca_oldteam,getteam(p));
            stuffcmd_flags(p, STUFFCMD_IGNOREINDEMO, "team \"%s\"\n", deadteam);
        }
    }
}

void CA_StoreTeams(void)
{
    gedict_t *p;
    for( p = world; (p = find_plr( p )); )
    {
        strcpy(p->ca_oldteam,getteam(p));
    }
}

void CA_RestoreTeams(void)
{
    gedict_t *p;
    for( p = world; (p = find_plr( p )); )
    {
        stuffcmd_flags(p, STUFFCMD_IGNOREINDEMO, "team \"%s\"\n", p->ca_oldteam);
    }
}

// called each time something/someone is killed.
void CA_killed_hook( gedict_t * killed, gedict_t * attacker )
{
    int alive = 0;

    if ( match_in_progress != 2 )
        return;

    if ( killed != attacker && CA_check_alive_teams( &alive ) < 2) {
        G_bprint (PRINT_MEDIUM, "%s had %d armour, %d health\n",
            attacker->s.v.netname,
            Q_rint(attacker->s.v.armorvalue),
            Q_rint(attacker->s.v.health));
    }

}

// CA Client Hook
void CA_client_think(void)
{
    if ( self->ct != ctPlayer )
        return;

    // if player dead, then allow speccing alive players with jump button.
    if ( !ISLIVE( self ) ) {
        CA_dead_jump_button();
    }
}

void CA_Frame(void)
{
	/*
		ra_match_fight values
		0 - Start the countdown
		1 - Countdown
		2 - Round in progress
		3 - Round complete, waiting for winner check
	*/

	static int last_r;
	static int last_s;

	int     r; // Round start timer
	int 	s; // Sudden death timer

	int 	alive_team = 0;

	gedict_t *p;

	if ( !isCA() || match_over )
		return;

	if ( match_in_progress != 2 )
		return;


	// check if there exist only one team with alive players and others are eliminated, if so then its time to start end timer
	if ( ra_match_fight == 2 )
    {
        if(CA_check_alive_teams( &alive_team ) <= 1 )
        {
		    time_to_end = g_globalvars.time + 3;

            CA_RestoreTeams();

    		ra_match_fight = 3;
    		return;
        }
        else
        {
            CA_SetDeadTeams();
        }
	}

	if ( ra_match_fight == 3 && Q_rint( time_to_end - g_globalvars.time ) <= 0)
	{
		switch ( CA_check_alive_teams( &alive_team ) )
		{
			case 0: // DRAW, both teams are dead
				{
					sound (world, CHAN_AUTO + CHAN_NO_PHS_ADD, "ca/sfdraw.wav", 1, ATTN_NONE);
                    G_cp2all("Round %d was a %s", round_num, redtext("draw"));
					break;
				}
			case 1: // Only one team alive
				{
                    G_cp2all("%s \x90%s\x91 wins round %d\n", redtext("Team"), cvar_string(va("_k_team%d", alive_team)),round_num);
                    G_bprint(2, "%s \x90%s\x91 wins round %d\n", redtext("Team"), cvar_string(va("_k_team%d", alive_team)),round_num);

                    CA_print_round_stats();

					if ( alive_team == 1 )
					{
						team1_score++;
					}
					else
					{
						team2_score++;
					}
					round_num++;
					break;
				}
			default: break; // both teams alive
		}

        ra_match_fight = 4;
        return;
	}

    if ( ra_match_fight == 4 && Q_rint( time_to_end - g_globalvars.time ) <= -3)
    {
        ra_match_fight = 0;
        return;
    }

	if ( team1_score >= CA_wins_required() || team2_score >= CA_wins_required() )
	{
		EndMatch( 0 );
		return;
	}

	if ( !ra_match_fight )
	{
		// ok start ra timer
		ra_match_fight = 1; // ra countdown
		last_r = 999999999;

		time_to_start  = g_globalvars.time + 9;

		for( p = world; (p = find_plr( p )); )
		{
			k_respawn( p, false );
		}
	}

	s = Q_rint( time_to_sudden_death - g_globalvars.time );

	if ( ra_match_fight == 2 && s <= 10 && s >= 0)
	{
		if ( s != last_s )
		{
			if ( s == 0 )
			{
				G_cp2all("Sudden death!");
			}
			else
			{
				if( s == 10 )
				{
					G_cp2all("Sudden death in 10 seconds");
				}
				if( s <= 5 )
				{
					G_cp2all("Sudden death in %d", s);
					sound (world, CHAN_AUTO + CHAN_NO_PHS_ADD, va("ca/sf%d.wav", s), 1, ATTN_NONE);
				}
			}
			last_s = s;
		}
	}

	if ( ra_match_fight == 2 && s <= 0 /* Set this to -10 later, if real sudden death is ever written */ )
	{
		if ( s != last_s )
		{
			// Start taking damage
			CA_damage_live_players(5);
			last_s = s;
		}
	}

	if ( ra_match_fight >= 2 ) // Match is in progress - no further action required
		return;

	r = Q_rint( time_to_start - g_globalvars.time );

	if ( r <= 0 )
	{
        CA_reset_round_stats();

        CA_StoreTeams();

		char *fight = redtext("FIGHT!");

		sound (world, CHAN_AUTO + CHAN_NO_PHS_ADD, "ca/sffight.wav", 1, ATTN_NONE);
		G_cp2all("%s\n\n\n\n", fight);

		ra_match_fight = 2;

		time_to_sudden_death = g_globalvars.time + 210;

		// rounding suck, so this force readytostart() return true right after FIGHT! is printed
		time_to_start = g_globalvars.time;
	}
	else if ( r != last_r )
	{
		last_r = r;

		if ( r == 7 )
		{
			sound (world, CHAN_AUTO + CHAN_NO_PHS_ADD, va("ca/sfround.wav"), 1, ATTN_NONE);
		}

		if ( r == 6 )
		{
			sound (world, CHAN_AUTO + CHAN_NO_PHS_ADD, va("ca/sf%d.wav", round_num), 1, ATTN_NONE);
		}

		if ( r < 6 )
		{
			sound (world, CHAN_AUTO + CHAN_NO_PHS_ADD, va("ca/sf%d.wav", r), 1, ATTN_NONE);
		}

		if ( r < 9 )
		{
			G_cp2all("Round %d of %d\n\n%s: %d\n\n"
				"\x90%s\x91:%s \x90%s\x91:%s",
				round_num, CA_rounds(), redtext("Countdown"), r, cvar_string("_k_team1"), dig3(team1_score), cvar_string("_k_team2"), dig3(team2_score)); // CA_wins_required
		}
	}
}
