///////////////////////////////////////////////////////////////////////
//  koogle_movement.c - This file contains all of the 
//                      movement routines for the koogle bot
//           
///////////////////////////////////////////////////////////////////////
#include "..\g_local.h"
#include "kooglebot.h"
#include "..\m_player.h"
#include "..\mq2_player.h"
///////////////////////////////////////////////////////////////////////
// Checks if bot can move (really just checking the ground)
// Also, this is not a real accurate check, but does a
// pretty good job and looks for lava/slime. 
///////////////////////////////////////////////////////////////////////
qboolean KOOGLEMV_CanMove(edict_t *self, int direction)
{
	vec3_t forward, right;
	vec3_t offset, start, end;
	vec3_t angles;
	trace_t tr;

	// Now check to see if move will move us off an edge
	VectorCopy(self->s.angles, angles);

	if (direction == MOVE_LEFT)
		angles[1] += 90;
	else if (direction == MOVE_RIGHT)
		angles[1] -= 90;
	else if (direction == MOVE_BACK)
		angles[1] -= 180;

	self->update_node = BOTNODE_JUMP;
	self->is_jumping = true;

	// Set up the vectors
	AngleVectors(angles, forward, right, NULL);

	VectorSet(offset, 36, 0, 24);
	G_ProjectSource(self->s.origin, offset, forward, right, start);

	VectorSet(offset, 36, 0, -400);
	G_ProjectSource(self->s.origin, offset, forward, right, end);

	tr = gi.trace(start, NULL, NULL, end, self, MASK_OPAQUE);

	if (tr.fraction > 0.3 && tr.fraction != 1 || tr.contents & (CONTENTS_LAVA | CONTENTS_SLIME))
	{
		if (debug_mode)
			debug_printf("%s: move blocked\n", self->client->pers.netname);
		return false;
	}

	return true; // yup, can move
}

///////////////////////////////////////////////////////////////////////
// Handle special cases of crouch/jump
//
// If the move is resolved here, this function returns
// true.
///////////////////////////////////////////////////////////////////////
qboolean KOOGLEMV_SpecialMove(edict_t *self, usercmd_t *ucmd)
{
	vec3_t dir, forward, right, start, end, offset;
	vec3_t top;
	trace_t tr;
	vec3_t dist;

	// Get current direction
	VectorCopy(self->client->ps.viewangles, dir);
	dir[YAW] = self->s.angles[YAW];
	AngleVectors(dir, forward, right, NULL);

	VectorSet(offset, 18, 0, 0);
	G_ProjectSource(self->s.origin, offset, forward, right, start);
	offset[0] += 18;
	G_ProjectSource(self->s.origin, offset, forward, right, end);

	// trace it
	start[2] += 18; // so they are not jumping all the time
	end[2] += 18;
	tr = gi.trace(start, self->mins, self->maxs, end, self, MASK_MONSTERSOLID);

	if (tr.allsolid)
	{
		// Check for crouching
		start[2] -= 14;
		end[2] -= 14;

		// Set up for crouching check
		VectorCopy(self->maxs, top);
		top[2] = 0.0; // crouching height
		tr = gi.trace(start, self->mins, top, end, self, MASK_PLAYERSOLID);

		// Crouch
		if (!tr.allsolid)
		{
			ucmd->forwardmove = 400;
			ucmd->upmove = -400;
			return true;
		}

		// Check for jump
		start[2] += 32;
		end[2] += 32;
		tr = gi.trace(start, self->mins, self->maxs, end, self, MASK_MONSTERSOLID);

		if (!tr.allsolid)
		{
			self->is_jumping = true;
			// Set up a jump move
			ucmd->forwardmove = 400;
			ucmd->upmove = 400;
			KOOGLEMV_ChangeBotAngle(self);
			VectorCopy(self->move_vector, dist);
			VectorScale(dist, 440, self->velocity);
			return true;
		}
	}

	return false; // We did not resolve a move here
}


///////////////////////////////////////////////////////////////////////
// Checks for obstructions in front of bot
//
// This is a function I created originally for KOOGLE that
// tries to help steer the bot around obstructions.
//
// If the move is resolved here, this function returns true.
///////////////////////////////////////////////////////////////////////
qboolean KOOGLEMV_CheckEyes(edict_t *self, usercmd_t *ucmd)
{
	vec3_t  forward, right;
	vec3_t  leftstart, rightstart, focalpoint;
	vec3_t  upstart, upend;
	vec3_t  dir, offset;
	vec3_t start, end, endp;
	vec3_t up;
	trace_t tr;
	trace_t traceRight, traceLeft, traceUp, traceFront; // for eyesight

	/*
	AngleVectors(self->owner->client->v_angle, forward, right, up);

	VectorSet(offset, 24, 6, self->owner->viewheight - 7);
	G_ProjectSource(self->owner->s.origin, offset, forward, right, start);
	VectorMA(start, 8192, forward, end);

	tr = gi.trace(start, NULL, NULL, end, self->owner, CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_DEADMONSTER);

	if (tr.fraction != 1) 
	{
		VectorMA(tr.endpos, -4, forward, endp);
		VectorCopy(endp, tr.endpos);
	}

	if ((tr.ent->svflags & SVF_MONSTER) || (tr.ent->client)) 
	{
		if ((tr.ent->takedamage) && (tr.ent != self->owner)) 
		{
			self->s.skinnum = 1;
		}
	}
	else
		self->s.skinnum = 0;

	vectoangles(tr.plane.normal, self->s.angles);
	VectorCopy(tr.endpos, self->s.origin);

	gi.linkentity(self);
	self->nextthink = level.time + 0.1;

	*/

	// Get current angle and set up "eyes"
	VectorCopy(self->s.angles, dir);
	AngleVectors(dir, forward, right, NULL);


	/* vec3_t offset is the 1st number */
	/* 200 is the 2nd number ( move forward 200 ) */
	/* 0 is the 3rd number is ( move to the right) */
	/* view height is the 4th number (actual view height) */

	// Let them move to targets by walls
	if (!self->movetarget)
	{                   
		VectorSet(offset, 200, 0, 4); 
	}                       
	
	VectorSet(offset, 36, 0, 4); // focalpoint 

	G_ProjectSource(self->s.origin, offset, forward, right, focalpoint);

	// Check from self to focalpoint
	// Ladder code
	VectorSet(offset, 36, 0, 0); // set as high as possible
	G_ProjectSource(self->s.origin, offset, forward, right, upend);
	traceFront = gi.trace(self->s.origin, self->mins, self->maxs, upend, self, MASK_OPAQUE);

	if (traceFront.contents & 0x8000000) // using detail brush here cuz sometimes it does not pick up ladders...??
	{
		ucmd->upmove = 400;
		ucmd->forwardmove = 400;
		return true;
	}

	// If this check fails we need to continue on with more detailed checks
	if (traceFront.fraction == 1)
	{
		ucmd->forwardmove = 400;
		return true;
	}

	VectorSet(offset, 0, 18, 4);
	G_ProjectSource(self->s.origin, offset, forward, right, leftstart);

	offset[1] -= 36; // want to make sure this is correct
	//VectorSet(offset, 0, -18, 4);
	G_ProjectSource(self->s.origin, offset, forward, right, rightstart);

	traceRight = gi.trace(rightstart, NULL, NULL, focalpoint, self, MASK_OPAQUE);
	traceLeft = gi.trace(leftstart, NULL, NULL, focalpoint, self, MASK_OPAQUE);

	// Wall checking code, this will degenerate progressively so the least cost 
	// check will be done first.

	// If open space move ok
	if (traceRight.fraction != 1 || traceLeft.fraction != 1 || strcmp(traceLeft.ent->classname, "func_door") != 0)
	{
		// Special uppoint logic to check for slopes/stairs/jumping etc.
		VectorSet(offset, 0, 18, 24);
		G_ProjectSource(self->s.origin, offset, forward, right, upstart);

		VectorSet(offset, 0, 0, 200); // scan for height above head
		G_ProjectSource(self->s.origin, offset, forward, right, upend);
		traceUp = gi.trace(upstart, NULL, NULL, upend, self, MASK_OPAQUE);

		VectorSet(offset, 200, 0, 200 * traceUp.fraction - 5); // set as high as possible
		G_ProjectSource(self->s.origin, offset, forward, right, upend);
		traceUp = gi.trace(upstart, NULL, NULL, upend, self, MASK_OPAQUE);

		// If the upper trace is not open, we need to turn.
		if (traceUp.fraction != 1)
		{
			if (traceRight.fraction > traceLeft.fraction)
				self->s.angles[YAW] += (1.0 - traceLeft.fraction) * 45.0;
			else
				self->s.angles[YAW] += -(1.0 - traceRight.fraction) * 45.0;

			ucmd->forwardmove = 400;
			return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////
// Make the change in angles a little more gradual, not so snappy
// Subtle, but noticeable.
// 
// Modified from the original id ChangeYaw code...
///////////////////////////////////////////////////////////////////////
void KOOGLEMV_ChangeBotAngle(edict_t *ent)
{
	float	ideal_yaw;
	float   ideal_pitch;
	float	current_yaw;
	float   current_pitch;
	float	move;
	float	speed;
	vec3_t  ideal_angle;

	// Normalize the move angle first
	VectorNormalize(ent->move_vector);

	current_yaw = anglemod(ent->s.angles[YAW]);
	current_pitch = anglemod(ent->s.angles[PITCH]);

	vectoangles(ent->move_vector, ideal_angle);

	ideal_yaw = anglemod(ideal_angle[YAW]);
	ideal_pitch = anglemod(ideal_angle[PITCH]);

	// Yaw
	if (current_yaw != ideal_yaw)
	{
		move = ideal_yaw - current_yaw;
		speed = ent->yaw_speed;
		if (ideal_yaw > current_yaw)
		{
			if (move >= 180)
				move = move - 360;
		}
		else
		{
			if (move <= -180)
				move = move + 360;
		}
		if (move > 0)
		{
			if (move > speed)
				move = speed;
		}
		else
		{
			if (move < -speed)
				move = -speed;
		}
		ent->s.angles[YAW] = anglemod(current_yaw + move);
	}

	// Pitch
	if (current_pitch != ideal_pitch)
	{
		move = ideal_pitch - current_pitch;
		speed = ent->yaw_speed;
		if (ideal_pitch > current_pitch)
		{
			if (move >= 180)
				move = move - 360;
		}
		else
		{
			if (move <= -180)
				move = move + 360;
		}
		if (move > 0)
		{
			if (move > speed)
				move = speed;
		}
		else
		{
			if (move < -speed)
				move = -speed;
		}
		ent->s.angles[PITCH] = anglemod(current_pitch + move);
	}
}

///////////////////////////////////////////////////////////////////////
// Set bot to move to it's movetarget. (following node path)
///////////////////////////////////////////////////////////////////////
void KOOGLEMV_MoveToGoal(edict_t *self, usercmd_t *ucmd)
{
	// If a rocket or grenade is around deal with it
	// Simple, but effective (could be rewritten to be more accurate)
	if (strcmp(self->movetarget->classname, "rocket") == 0 ||
		strcmp(self->movetarget->classname, "grenade") == 0)
	{
		VectorSubtract(self->movetarget->s.origin, self->s.origin, self->move_vector);
		KOOGLEMV_ChangeBotAngle(self);
		if (debug_mode)
			debug_printf("%s: Oh crap a rocket!\n", self->client->pers.netname);

		// strafe left/right
		if (rand() % 1 && KOOGLEMV_CanMove(self, MOVE_LEFT))
			ucmd->sidemove = -400;
		else if (KOOGLEMV_CanMove(self, MOVE_RIGHT))
			ucmd->sidemove = 400;
		return;
	}
	else
	{
		// Set bot's movement direction
		VectorSubtract(self->movetarget->s.origin, self->s.origin, self->move_vector);
		KOOGLEMV_ChangeBotAngle(self);
		ucmd->forwardmove = 400;
		return;
	}
}

///////////////////////////////////////////////////////////////////////
// Main movement code. (following node path)
///////////////////////////////////////////////////////////////////////
void KOOGLEMV_Move(edict_t *self, usercmd_t *ucmd)
{
	vec3_t dist;
	int current_node_type =-1;
	int next_node_type =-1;
	int i;
	float c;

	// Get current and next node back from nav code.
	if (!KOOGLEND_FollowPath(self))
	{
		self->state = BOTSTATE_WANDER;
		self->wander_timeout = level.time + 1.0;
		return;
	}

	current_node_type = nodes[self->current_node].type;
	next_node_type = nodes[self->next_node].type;

	///////////////////////////
	// Move To Goal
	///////////////////////////
	if (self->movetarget)
		KOOGLEMV_MoveToGoal(self, ucmd);

	////////////////////////////////////////////////////////
	// Grapple
	///////////////////////////////////////////////////////
	//if(next_node_type == BOTNODE_GRAPPLE)
	//{
	//KOOGLEMV_ChangeBotAngle(self);
	//KOOGLEIT_ChangeWeapon(self,FindItem("grapple"));
	//ucmd->buttons = BUTTON_ATTACK;
	//return;
	//}
	// Reset the grapple if hanging on a grapple node
	//if(current_node_type == BOTNODE_GRAPPLE)
	//{
	//CTFPlayerResetGrapple(self);
	//return;
	//}

	////////////////////////////////////////////////////////
	// Platforms
	///////////////////////////////////////////////////////
	if (current_node_type != BOTNODE_PLATFORM && next_node_type == BOTNODE_PLATFORM)
	{
		// check to see if lift is down?
		for (i = 0; i<num_items; i++)
			if (item_table[i].node == self->next_node)
				if (item_table[i].ent->moveinfo.state != STATE_BOTTOM)
					return; // Wait for elevator
	}
	if (current_node_type == BOTNODE_PLATFORM && next_node_type == BOTNODE_PLATFORM)
	{
		// Move to the center
		self->move_vector[2] = 0; // kill z movement	
		if (VectorLength(self->move_vector) > 10)
			ucmd->forwardmove = 200; // walk to center

		KOOGLEMV_ChangeBotAngle(self);

		return; // No move, riding elevator
	}

	////////////////////////////////////////////////////////
	// Jumpto Nodes
	///////////////////////////////////////////////////////
	if (next_node_type == BOTNODE_JUMP ||
		(current_node_type == BOTNODE_JUMP && next_node_type != BOTNODE_ITEM && nodes[self->next_node].origin[2] > self->s.origin[2]))
	{
		self->is_jumping = true;
		// Set up a jump move
		ucmd->forwardmove = 400;
		ucmd->upmove = 400;
		KOOGLEMV_ChangeBotAngle(self);
		VectorCopy(self->move_vector, dist);
		VectorScale(dist, 440, self->velocity);
		return;
	}

	////////////////////////////////////////////////////////
	// Crouch to Nodes
	///////////////////////////////////////////////////////
	//if (next_node_type == BOTNODE_CROUCH ||
	//	(current_node_type == BOTNODE_CROUCH && next_node_type != BOTNODE_ITEM && nodes[self->next_node].origin[2] > self->s.origin[2]))
	//{
		// Set up a jump move
		//ucmd->forwardmove = 400;
		//ucmd->upmove = -400;
		//return;

	    //if (current_node_type == BOTNODE_CROUCH || next_node_type == BOTNODE_CROUCH)
	    //{
		 //  ucmd->upmove = -400;
	    //}

		if (next_node_type == BOTNODE_CROUCH && nodes[self->next_node].origin[2] > self->s.origin[2])
		{
			c = random();

			if (c < 0.2 && KOOGLEMV_CanMove(self, MOVE_LEFT))
			{
				ucmd->sidemove -= 400;
				ucmd->upmove = -400;
				// is it a crouching node?
				if (self->maxs[2] > DUCKING_MAX_Z)
					self->maxs[2] = DUCKING_MAX_Z;
				return;
			}
			else if (c < 0.4 && KOOGLEMV_CanMove(self, MOVE_RIGHT))
			{
				ucmd->sidemove += 400;
				ucmd->upmove = -400;
				// is it a crouching node?
				if (self->maxs[2] > DUCKING_MAX_Z)
					self->maxs[2] = DUCKING_MAX_Z;
				return;
			}
			if (c < 0.6 && KOOGLEMV_CanMove(self, MOVE_FORWARD))
			{
				ucmd->forwardmove += 400;
				ucmd->upmove = -400;
				// is it a crouching node?
				if (self->maxs[2] > DUCKING_MAX_Z)
					self->maxs[2] = DUCKING_MAX_Z;
				return;
			}
			else if (c < 0.8 && KOOGLEMV_CanMove(self, MOVE_FORWARD))
			{
				ucmd->forwardmove = 400;
				ucmd->upmove = -400;
				// is it a crouching node?
				if (self->maxs[2] > DUCKING_MAX_Z)
					self->maxs[2] = DUCKING_MAX_Z;
				return;
			}

		}
	//}

	////////////////////////////////////////////////////////
	// Ladder Nodes
	///////////////////////////////////////////////////////
	if (next_node_type == BOTNODE_LADDER && nodes[self->next_node].origin[2] > self->s.origin[2])
	{
		// Otherwise move as fast as we can
		ucmd->forwardmove = 400;
		self->velocity[2] = 320;

		KOOGLEMV_ChangeBotAngle(self);
		return;
	}
	// If getting off the ladder
	if (current_node_type == BOTNODE_LADDER && next_node_type != BOTNODE_LADDER &&
		nodes[self->next_node].origin[2] > self->s.origin[2])
	{
		ucmd->forwardmove = 400;
		ucmd->upmove = 200;
		self->velocity[2] = 200;
		KOOGLEMV_ChangeBotAngle(self);
		return;
	}

	////////////////////////////////////////////////////////
	// Water Nodes
	///////////////////////////////////////////////////////
	if (current_node_type == BOTNODE_WATER)
	{
		// We need to be pointed up/down
		KOOGLEMV_ChangeBotAngle(self);

		// If the next node is not in the water, then move up to get out.
		if (next_node_type != BOTNODE_WATER && !(gi.pointcontents(nodes[self->next_node].origin) & MASK_WATER)) // Exit water
			ucmd->upmove = 400;

		ucmd->forwardmove = 300;
		return;

	}

	// Falling off ledge?
	if (!M_CheckBottom(self) || !self->groundentity)
	{
		KOOGLEMV_ChangeBotAngle(self);
		self->velocity[0] = self->move_vector[0] * 360;
		self->velocity[1] = self->move_vector[1] * 360;
		return;
	}

	// Check to see if stuck, and if so try to free us
	// Also handles crouching
	if (VectorLength(self->velocity) < 37)
	{
		// Keep a random factor just in case....
		if (random() > 0.1 && KOOGLEMV_SpecialMove(self, ucmd))
			return;

		self->s.angles[YAW] += random() * 180 - 90;

		ucmd->forwardmove = 400;

		return;
	}

	// Otherwise move as fast as we can
	ucmd->forwardmove = 400;
	KOOGLEMV_ChangeBotAngle(self);
}

///////////////////////////////////////////////////////////////////////
// Wandering code (based on old KOOGLE movement code) 
///////////////////////////////////////////////////////////////////////
void KOOGLEMV_Wander(edict_t *self, usercmd_t *ucmd)
{
	vec3_t  temp;

	// Do not move
	if (self->next_move_time > level.time)
		return;

	// Special check for elevators, stand still until the ride comes to a complete stop.
	if (self->groundentity != NULL && self->groundentity->use == Use_Plat)
		if (self->groundentity->moveinfo.state == STATE_UP ||
			self->groundentity->moveinfo.state == STATE_DOWN) // only move when platform not
		{
			self->velocity[0] = 0;
			self->velocity[1] = 0;
			self->velocity[2] = 0;
			self->next_move_time = level.time + 0.5;
			return;
		}

	// Is there a target to move to
	if (self->movetarget)
		KOOGLEMV_MoveToGoal(self, ucmd);

	////////////////////////////////
	// Swimming?
	////////////////////////////////
	VectorCopy(self->s.origin, temp);
	temp[2] += 24;

	if (gi.pointcontents(temp) & MASK_WATER)
	{
		// If drowning and no node, move up
		if (self->client->next_drown_time > 0)
		{
			ucmd->upmove = 1;
			self->s.angles[PITCH] = -45;
		}
		else
			ucmd->upmove = 15;

		ucmd->forwardmove = 300;
	}
	else
		self->client->next_drown_time = 0; // probably should not be messing with this, but

	////////////////////////////////
	// Lava?
	////////////////////////////////
	temp[2] -= 48;
	if (gi.pointcontents(temp) & (CONTENTS_LAVA | CONTENTS_SLIME))
	{
		//	safe_bprintf(PRINT_MEDIUM,"lava jump\n");
		self->s.angles[YAW] += random() * 360 - 180;
		ucmd->forwardmove = 400;
		ucmd->upmove = 400;
		return;
	}

	if (KOOGLEMV_CheckEyes(self, ucmd))
		return;

	// Check for special movement if we have a normal move (have to test)
	if (VectorLength(self->velocity) < 37)
	{
		if (random() > 0.1 && KOOGLEMV_SpecialMove(self, ucmd))
			return;

		self->s.angles[YAW] += random() * 180 - 90;

		if (!M_CheckBottom(self) || !self->groundentity) // if there is ground continue otherwise wait for next move
			ucmd->forwardmove = 400;

		return;
	}

	ucmd->forwardmove = 400;

}

///////////////////////////////////////////////////////////////////////
// Attack movement routine
//
// NOTE: Very simple for now, just a basic move about avoidance.
//       Change this routine for more advanced attack movement.
///////////////////////////////////////////////////////////////////////
qboolean is_bot;

void KOOGLEMV_Attack(edict_t *self, usercmd_t *ucmd)
{
	float c;
	vec3_t  target;
	vec3_t  angles;

	if (is_bot)
		return;

	// Randomly choose a movement direction
	c = random();

	if (c < 0.2 && KOOGLEMV_CanMove(self, MOVE_LEFT))
		ucmd->sidemove -= 400;
	else if (c < 0.4 && KOOGLEMV_CanMove(self, MOVE_RIGHT))
		ucmd->sidemove += 400;

	if (c < 0.6 && KOOGLEMV_CanMove(self, MOVE_FORWARD))
		ucmd->forwardmove += 400;
	else if (c < 0.8 && KOOGLEMV_CanMove(self, MOVE_FORWARD))
		ucmd->forwardmove -= 400;

	if (c < 0.95)
		ucmd->upmove -= 200;
	else
		ucmd->upmove += 200;

	// Set the attack 
	ucmd->buttons = BUTTON_ATTACK;

	// Aim
	VectorCopy(self->enemy->s.origin, target);

	// modify attack angles based on accuracy (mess this up to make the bot's aim not so deadly)
	target[0] += (random() - 0.5) * 20;
	target[1] += (random() - 0.5) * 20;

	// Set direction
	VectorSubtract(target, self->s.origin, self->move_vector);
	vectoangles(self->move_vector, angles);
	VectorCopy(angles, self->s.angles);

	if(debug_mode)
	debug_printf("%s attacking %s\n",self->client->pers.netname,self->enemy->client->pers.netname);
}

