// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/remote_commands_state.h"

#include "base/logging.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

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
  current_id_ = kInitId;
  g_last_acked_id_ = kInitId;
}

int64_t RemoteCommandsState::AddPendingRemoteCommand(
    const em::RemoteCommand& command) {
  base::AutoLock lock(lock_);
  ++current_id_;
  // Copy the incoming command to be able to set a `command_id`.
  enterprise_management::RemoteCommand command_copy = command;
  command_copy.set_command_id(current_id_);
  pending_commands_.push_back(command_copy);
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

std::vector<em::RemoteCommand>
RemoteCommandsState::ExtractPendingRemoteCommands() {
  base::AutoLock lock(lock_);
  std::vector<em::RemoteCommand> pending_commands_tmp(
      std::move(pending_commands_));
  pending_commands_.clear();
  return pending_commands_tmp;
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
