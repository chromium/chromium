// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/remote_commands_state.h"

#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

RemoteCommandsState::RemoteCommandsState() {
  ResetState();
}
RemoteCommandsState::RemoteCommandsState(RemoteCommandsState&& state) = default;
RemoteCommandsState& RemoteCommandsState::operator=(
    RemoteCommandsState&& state) = default;
RemoteCommandsState::~RemoteCommandsState() = default;

void RemoteCommandsState::ResetState() {
  command_results.clear();
  ClearPendingRemoteCommands();
}

void RemoteCommandsState::ClearPendingRemoteCommands() {
  pending_commands.clear();
}

void RemoteCommandsState::AddPendingRemoteCommand(
    const em::RemoteCommand& command) {
  pending_commands.push_back(command);
}

void RemoteCommandsState::AddRemoteCommandResult(
    const em::RemoteCommandResult& result) {
  command_results[result.command_id()] = result;
}

const std::vector<em::RemoteCommand>&
RemoteCommandsState::GetPendingRemoteCommands() const {
  return pending_commands;
}

bool RemoteCommandsState::GetRemoteCommandResult(
    int id,
    em::RemoteCommandResult* result) {
  if (command_results.count(id) == 0) {
    return false;
  }

  *result = command_results.at(id);

  return true;
}

}  // namespace policy
