// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/remote_commands_state.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

IssuedRemoteCommand::IssuedRemoteCommand() = default;
IssuedRemoteCommand::IssuedRemoteCommand(const IssuedRemoteCommand&) = default;
IssuedRemoteCommand& IssuedRemoteCommand::operator=(
    const IssuedRemoteCommand&) = default;
IssuedRemoteCommand::~IssuedRemoteCommand() = default;

const int64_t kInitId = 0;
const int kCommandIdMaxDistance = 100;

RemoteCommandsState::RemoteCommandsState()
    : observer_list_(
          base::MakeRefCounted<base::ObserverListThreadSafe<Observer>>()) {
  ResetState();
}
RemoteCommandsState::~RemoteCommandsState() = default;

void RemoteCommandsState::AddObserver(Observer* observer) {
  base::AutoLock lock(lock_);
  observer_list_->AddObserver(observer);
}

void RemoteCommandsState::RemoveObserver(Observer* observer) {
  base::AutoLock lock(lock_);
  observer_list_->RemoveObserver(observer);
}

void RemoteCommandsState::ResetState() {
  base::AutoLock lock(lock_);
  command_results_.clear();
  pending_commands_.clear();
  issued_commands_.clear();
  current_id_ = kInitId;
  g_last_acked_id_ = kInitId;
}

int64_t RemoteCommandsState::AddPendingRemoteCommand(
    const em::RemoteCommand& command,
    RemoteCommandScope scope) {
  base::AutoLock lock(lock_);
  ++current_id_;
  // Copy the incoming command to be able to set a `command_id`.
  enterprise_management::RemoteCommand command_copy = command;
  command_copy.set_command_id(current_id_);
  observer_list_->Notify(FROM_HERE,
                         &RemoteCommandsState::Observer::OnRemoteCommandAdded,
                         current_id_, command_copy, scope);
  issued_commands_[command_copy.command_id()] = command_copy;
  pending_commands_.push_back(
      QueuedRemoteCommand{std::move(command_copy), scope});
  return current_id_;
}

void RemoteCommandsState::AddRemoteCommandResult(
    const em::RemoteCommandResult& result) {
  base::AutoLock lock(lock_);
  command_results_[result.command_id()] = result;
  observer_list_->Notify(
      FROM_HERE, &RemoteCommandsState::Observer::OnRemoteCommandResultAvailable,
      result.command_id());
}

void RemoteCommandsState::AddRemoteCommandAcked(const int64_t command_id) {
  base::AutoLock lock(lock_);
  if ((command_id > g_last_acked_id_) &&
      (command_id - g_last_acked_id_ < kCommandIdMaxDistance)) {
    for (int64_t i = g_last_acked_id_ + 1; i <= command_id; i++) {
      observer_list_->Notify(
          FROM_HERE, &RemoteCommandsState::Observer::OnRemoteCommandAcked, i);
    }
  } else {
    LOG(WARNING) << "The number of commands to be acked is too large. Only "
                    "acknowledging command id: "
                 << command_id;
    observer_list_->Notify(FROM_HERE,
                           &RemoteCommandsState::Observer::OnRemoteCommandAcked,
                           command_id);
  }
  g_last_acked_id_ = std::max(g_last_acked_id_, command_id);
}

std::vector<IssuedRemoteCommand> RemoteCommandsState::GetIssuedCommands() {
  base::AutoLock lock(lock_);
  std::vector<IssuedRemoteCommand> commands;
  for (const auto& [id, command] : issued_commands_) {
    IssuedRemoteCommand& issued = commands.emplace_back();
    issued.command = command;
    if (auto it = command_results_.find(id); it != command_results_.end()) {
      issued.status = IssuedRemoteCommand::Status::kResultAvailable;
      issued.result = it->second;
    } else if (std::ranges::none_of(pending_commands_, [id](const auto& q) {
                 return q.command.command_id() == id;
               })) {
      issued.status = IssuedRemoteCommand::Status::kFetched;
    }
  }
  return commands;
}

std::vector<em::RemoteCommand>
RemoteCommandsState::ExtractPendingRemoteCommands(
    RemoteCommandScope client_scope) {
  base::AutoLock lock(lock_);
  std::vector<em::RemoteCommand> matched_commands;
  std::vector<QueuedRemoteCommand> remaining_commands;
  for (auto& queued : pending_commands_) {
    if (queued.scope == RemoteCommandScope::kAny ||
        client_scope == RemoteCommandScope::kAny ||
        queued.scope == client_scope) {
      matched_commands.push_back(std::move(queued.command));
    } else {
      remaining_commands.push_back(std::move(queued));
    }
  }
  pending_commands_ = std::move(remaining_commands);
  return matched_commands;
}

bool RemoteCommandsState::GetRemoteCommandResult(
    int64_t id,
    em::RemoteCommandResult* result) {
  base::AutoLock lock(lock_);
  if (command_results_.count(id) == 0) {
    return false;
  }

  *result = command_results_.at(id);

  return true;
}

bool RemoteCommandsState::IsRemoteCommandResultAvailable(int64_t id) {
  base::AutoLock lock(lock_);
  if (command_results_.count(id) == 0) {
    return false;
  }
  return true;
}

bool RemoteCommandsState::IsRemoteCommandAcked(int64_t id) {
  base::AutoLock lock(lock_);
  if (id <= g_last_acked_id_) {
    return true;
  }
  return false;
}

void RemoteCommandsState::SetCurrentIdForTesting(int64_t id) {
  base::AutoLock lock(lock_);
  current_id_ = id;
  g_last_acked_id_ = id;
}

}  // namespace policy
