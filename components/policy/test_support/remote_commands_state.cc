// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/remote_commands_state.h"

#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

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
}

void RemoteCommandsState::AddPendingRemoteCommand(
    const em::RemoteCommand& command) {
  base::AutoLock lock(lock_);
  pending_commands_.push_back(command);
}

void RemoteCommandsState::AddRemoteCommandResult(
    const em::RemoteCommandResult& result) {
  base::AutoLock lock(lock_);
  command_results_[result.command_id()] = result;
  observer_list_->Notify(
      FROM_HERE, &RemoteCommandsState::Observer::OnRemoteCommandResultAvailable,
      result.command_id());
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

}  // namespace policy
