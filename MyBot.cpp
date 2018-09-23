#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include "Game_Api.h"
using json = nlohmann::json;
using Player = Game_Api::Player;
using Monster = Game_Api::Monster;
using DeathEffects = Game_Api::DeathEffects;

#define RESPONSE_SECS 1
#define RESPONSE_NSECS 0

#include <iostream>
using namespace std;

//Globals
Game_Api * api;
int my_player_num = 0;

node_id_t destination_decision;
string stance;
int turn;

string get_beating_stance(string stance){
	if (stance == "Rock") return "Paper";
	if (stance == "Paper") return "Scissors";
	return "Rock";
}

string random_stance(){
	switch(rand()%3){
	case 0: return "Rock";
	case 1: return "Paper";
	case 2: return "Scissors";
	}
	return "Rock";
}
string random_except_stance(string except){
	string ret;
	do{
		ret = random_stance();
	}while(ret == except);
	return ret;
}

//node_id_t open_location_arr[] = {10, 16, 12, 22, 21, 20, 13, 4, 2, 3, 1, 0};
//int       open_duration_arr[] = { 7,  8,  8,  7,  7,  6,  7, 5, 6, 5, 4, 4};
node_id_t open_location_arr[] = {10, 16, 12, 22, 21, 20, 13, 12, 16, 10, 0};
int       open_duration_arr[] = { 7,  8,  8,  7,  7,  6,  7,  5,  5,  5, 5};
node_id_t ring_location_arr[] = {10, 16, 15, 18, 17, 16, 10, 0};
node_id_t save_location_arr[] = {1, 3, 1, 0};
int       save_duration_arr[] = {5, 5, 4, 4};
node_id_t quik_location_arr[] = {10, 11, 10, 0};
node_id_t medi_location_arr[] = {10, 9, 8, 7, 6, 0};
int location_step = 0;
int step_turn = 0;
int state = 0; // 0 early, 1 camp health, 2 farm ring, 3 pickup speed 3, 4 quik farming, 5 medi farming

char pht[81];
char gsr = 0;

char get_stance_bits(string stance){
	if (stance == "Rock") return 0;
	if (stance == "Paper") return 1;
	return 2;
}
char get_next_tri_counter(char counter, char next){
	if(next == 0){
		switch(counter){
		case 0: case 4: case 5: return 0;
		case 1: case 2: case 3: return counter-1;
		case 7: return 4;
		case 9: return 7;
		}
		return 5;
	}else if(next == 1){
		switch(counter){
		case 3: case 6: case 5: return 3;
		case 0: case 1: case 2: return counter+1;
		case 8: return 6;
		case 9: return 8;
		}
		return 5;
	}else{
		switch(counter){
		case 9: case 7: case 8: case 5: return 9;
		case 0: return 4;
		case 4: return 7;
		case 3: return 6;
		case 6: return 8;
		}
		return 5;
	}
}

string predict_stance(){
	switch(pht[gsr]){
	case 0: case 1: case 4: return "Paper";
	case 2: case 3: case 6: return "Scissors";
	case 7: case 8: case 9: return "Rock";
	}
	return random_stance();
}

void early_strat(){
	if(location_step+2 == sizeof(open_duration_arr)/sizeof(node_id_t) && step_turn == 1 && api->get_self()._speed < 3){
		api->log("Missed 3: "+to_string(api->get_monster(3)._respawn_counter)+", 0: "+to_string(api->get_monster(0)._respawn_counter));
		if(api->get_monster(3)._respawn_counter>50 || (api->get_monster(0)._respawn_counter >= 7 &&
				api->get_monster(0)._respawn_counter-api->get_monster(3)._respawn_counter<8)){
			open_duration_arr[location_step]++;
			open_duration_arr[location_step+1]++;
		}else{
			open_duration_arr[location_step] += 1+api->get_monster(3)._respawn_counter;
		}
	}
	if(open_duration_arr[location_step] == step_turn++) {
		location_step++;
		step_turn = 1;
	}
	if (open_duration_arr[location_step] - step_turn < 7 - api->get_self()._speed)
		destination_decision = open_location_arr[location_step];
	else
		destination_decision = api->get_self()._location;
	
	if(location_step+1 == sizeof(open_duration_arr)/sizeof(node_id_t) 
			&& step_turn == open_duration_arr[location_step]){
		state = 1;
	}
}

void farm_strat(node_id_t* farm_location_arr, int len){
	if(7-api->get_self()._speed == step_turn++) {
		location_step++;
		step_turn = 1;
	}
	node_id_t loc = api->get_self()._location;
	if(location_step+2 == len && step_turn == 1 && loc != api->get_opponent()._location
			&& api->get_monster(0)._respawn_counter > 2*(7-api->get_self()._speed)){
		destination_decision = loc;
		step_turn--;
	}else
		destination_decision = farm_location_arr[location_step];

	if(location_step+1 == len && step_turn == 7-api->get_self()._speed){
		state = 1;
	}
}

void save_strat(){
	if(location_step+2 == sizeof(save_duration_arr)/sizeof(node_id_t) && step_turn == 1 && api->get_self()._speed < 3){
		save_duration_arr[location_step]++;
		save_duration_arr[location_step+1]++;
	}
	if(save_duration_arr[location_step] == step_turn++) {
		location_step++;
		step_turn = 1;
	}
	if (save_duration_arr[location_step] - step_turn < 7 - api->get_self()._speed)
		destination_decision = save_location_arr[location_step];
	else
		destination_decision = api->get_self()._location;
	
	if(location_step+1 == sizeof(save_duration_arr)/sizeof(node_id_t) 
			&& step_turn == save_duration_arr[location_step]){
		state = 1;
	}
}

void camp_strat(){
	destination_decision = 0;
	
	// check for HP camper
	Player me = api->get_self();
	Player opponent = api->get_opponent();
	int mySum  = me._rock+me._paper+me._scissors;
	int oppSum = opponent._rock+opponent._paper+opponent._scissors;
	if(opponent._location == 0 && mySum*me._health > oppSum*opponent._health){
		return;
	}
	
	// check for missions
	/*if (me._speed < 3){
		if ((!api->get_monster(3)._dead || api->get_monster(3)._respawn_counter < 10) &&
				api->get_monster(0)._respawn_counter > 20){
			state = 3;
			location_step = 0;
			step_turn = 0;
			save_strat();
		}
	}else*/ if ((!api->get_monster(17)._dead || api->get_monster(17)._respawn_counter < 5*(7-me._speed)) &&
		(!api->get_monster(15)._dead || api->get_monster(15)._respawn_counter < 3*(7-me._speed)) && 
				((api->get_monster(0)._respawn_counter > 8*(7-me._speed) && me._health > 40) ||
				(api->get_monster(0)._respawn_counter < (6-me._speed) && me._health > 20))){
		state = 2;
		location_step = 0;
		step_turn = 0;
		farm_strat(ring_location_arr, sizeof(ring_location_arr)/sizeof(node_id_t));
	}else if ((!api->get_monster(8)._dead || api->get_monster(8)._respawn_counter < 3*(7-me._speed)) && 
				((api->get_monster(0)._respawn_counter > 6*(7-me._speed) && me._health > 30) ||
				(api->get_monster(0)._respawn_counter < (6-me._speed) && me._health > 10))){
		state = 5;
		location_step = 0;
		step_turn = 0;
		farm_strat(medi_location_arr, sizeof(medi_location_arr)/sizeof(node_id_t));
	}else if ((!api->get_monster(11)._dead || api->get_monster(11)._respawn_counter < 3*(7-me._speed)) && 
				((api->get_monster(0)._respawn_counter > 4*(7-me._speed) && me._health > 30) ||
				(api->get_monster(0)._respawn_counter < (6-me._speed) && me._health > 10))){
		state = 4;
		location_step = 0;
		step_turn = 0;
		farm_strat(quik_location_arr, sizeof(quik_location_arr)/sizeof(node_id_t));
	}/*else if(me._health < 20 && api->get_monster(0)._respawn_counter > 8){
		state = 6;
		location_step = 1;
		step_turn = 0;
	}*/
}

void strategy(){
	Player me = api->get_self();
	Player opponent = api->get_opponent();
	turn = api->get_turn_num();
	destination_decision = me._location;
	
	// deal with pht
	int stance_bits = get_stance_bits(opponent._stance);
	if(turn != 1)
		pht[gsr] = get_next_tri_counter(pht[gsr], stance_bits);
	gsr = (gsr*3)%81 + stance_bits;
	
	// determine destination
	switch(state){
	case 0: // early game
		early_strat();
		break;
	case 1: // camp health
		camp_strat();
		break;
	case 2: // farm ring
		farm_strat(ring_location_arr, sizeof(ring_location_arr)/sizeof(node_id_t));
		break;
	case 3: // save speed 3
		save_strat();
		break;
	case 4: // quik farming
		farm_strat(quik_location_arr, sizeof(quik_location_arr)/sizeof(node_id_t));
		break;
	case 5: // medi farming
		farm_strat(medi_location_arr, sizeof(medi_location_arr)/sizeof(node_id_t));
		break;
	}
	
	// determine stance
	node_id_t stance_location = me._location;
	if (me._movement_counter-1 == me._speed && me._destination == destination_decision){
		stance_location = destination_decision;
	}
	stance = predict_stance();
	
	/*if (opponent._rock >= 2*opponent._paper && opponent._rock >= 2*opponent._scissors
			&& stance == "Scissors"){
		stance = random_except_stance("Scissors");
	}else if (opponent._paper >= 2*opponent._rock && opponent._paper >= 2*opponent._scissors
			&& stance == "Rock"){
		stance = random_except_stance("Rock");
	}else if (opponent._scissors >= 2*opponent._paper && opponent._scissors >= 2*opponent._rock
			&& stance == "Paper"){
		stance = random_except_stance("Paper");
	}*/
	
	if (turn <= 300 && api->has_monster(stance_location)){
		Monster monster = api->get_monster(stance_location);
		if (!monster._dead || monster._respawn_counter == 1)
			stance = get_beating_stance(monster._stance);
	}
	//if(stance_location == 3) stance = "Rock";
	
	api->log(to_string(turn)+") goto "+to_string(destination_decision)+" with "+stance);
	api->submit_decision(destination_decision, stance);
}

int main() {
	srand(time(NULL));
	for(int i=0; i<81; i++){
		pht[i] = 5;
	}
	while(1){
		char* buf = NULL;
		size_t size = 0;
		getline(&buf, &size, stdin);
		json data = json::parse(buf);
		if(data["type"] == "map"){
			my_player_num = data["player_id"];
			api = new Game_Api(my_player_num, data["map"]);
		} else {
			api->update(data["game_data"]);
			strategy();
            fflush(stdout);
			free(buf);
	    }
	}
}
