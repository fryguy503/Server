#include "shared_tasks.h"
#include "clientlist.h"
#include "cliententry.h"
#include "zonelist.h"

#include <algorithm>

extern ClientList client_list;
extern ZSList zoneserver_list;

void SharedTaskManager::HandleTaskRequest(ServerPacket *pack)
{
	if (!pack)
		return;

	char tmp_str[64] = { 0 };
	int task_id = pack->ReadUInt32();
	pack->ReadString(tmp_str);
	std::string leader_name = tmp_str;
	int player_count = pack->ReadUInt32();
	std::vector<std::string> players;
	for (int i = 0; i < player_count; ++i) {
		pack->ReadString(tmp_str);
		players.push_back(tmp_str);
	}

	// check if the task exist, we only load shared tasks in world, so we know the type is correct if found
	auto it = task_information.find(task_id);
	if (it == task_information.end()) { // not loaded! bad id or not shared task
		auto pc = client_list.FindCharacter(leader_name.c_str());
		if (pc) {
			auto pack = new ServerPacket(ServerOP_TaskReject, leader_name.size() + 1 + 4);
			pack->WriteUInt32(0); // string ID or just generic fail message
			pack->WriteString(leader_name.c_str());
			zoneserver_list.SendPacket(pc->zone(), pc->instance(), pack);
			safe_delete(pack);
		} // oh well
		return;
	}

	int id = GetNextID();
	auto ret = tasks.insert({id, {id, task_id}});
	if (!ret.second) {
		auto pc = client_list.FindCharacter(leader_name.c_str());
		if (pc) {
			auto pack = new ServerPacket(ServerOP_TaskReject, leader_name.size() + 1 + 4);
			pack->WriteUInt32(0); // string ID or just generic fail message
			pack->WriteString(leader_name.c_str());
			zoneserver_list.SendPacket(pc->zone(), pc->instance(), pack);
			safe_delete(pack);
		} // oh well
		return;
	}

	auto cle = client_list.FindCharacter(leader_name.c_str());
	if (cle == nullptr) {// something went wrong
		tasks.erase(ret.first);
		return;
	}

	auto &task = ret.first->second;
	task.AddMember(leader_name, cle, true);

	if (players.empty()) {
		// send instant success to leader
		SerializeBuffer buf(10);
		buf.WriteInt32(id);				// task's ID
		buf.WriteString(leader_name);	// leader's name

		auto pack = new ServerPacket(ServerOP_TaskGrant, buf);
		zoneserver_list.SendPacket(cle->zone(), cle->instance(), pack);
		safe_delete(pack);
		return;
	}

	for (auto &&name : players) {
		// look up CLEs by name, tell them we need to know if they can be added
		cle = client_list.FindCharacter(name.c_str());
		if (cle) {
			// make sure we don't have a shared task already

			// make sure our level is right

			// check our lock out timer

			// we're good, add to task
			task.AddMember(name, cle);
		}
	}
}

/*
 * This is called once during boot of world
 * We need to load next_id, clean up expired tasks (?), and populate the map
 */
bool SharedTaskManager::LoadSharedTaskState()
{
	// clean up expired tasks ... We may not want to do this if world crashes and has to be restarted
	// May need to wait to do it to cleanly inform existing zones to clean up expired tasks

	// Load existing tasks. We may not want to actually do this here and wait for a client to log in
	// But the crash case may actually dictate we should :P

	// set next_id to highest used ID + 1
	return true;
}

int SharedTaskManager::GetNextID()
{
	next_id++;
	// let's not be extra clever here ...
	while (tasks.count(next_id) != 0)
		next_id++;

	return next_id;
}

/*
 * When a player leaves world they will tell us to clean up their pointer
 * This is NOT leaving the shared task, just crashed or something
 */

void SharedTask::MemberLeftGame(ClientListEntry *cle)
{
	auto it = std::find_if(members.begin(), members.end(), [cle](SharedTaskMember &m) { return m.cle == cle; });

	// ahh okay ...
	if (it == members.end())
		return;

	it->cle = nullptr;
}

