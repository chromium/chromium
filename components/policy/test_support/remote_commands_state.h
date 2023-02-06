// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REMOTE_COMMANDS_STATE_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REMOTE_COMMANDS_STATE_H_

#include "base/containers/flat_map.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

// Stores information about current pending remote commands, and contains
// execution results of sent remote commands.
class RemoteCommandsState {
 public:
  RemoteCommandsState();
  RemoteCommandsState(RemoteCommandsState&& state);
  RemoteCommandsState& operator=(RemoteCommandsState&& state);
  ~RemoteCommandsState();

  // Removes all pending remote commands and available results.
  // Gets called on fake_dmserver construction.
  void ResetState();

  // Removes all pending remote commands.
  // This is intended to be used to clean up commands after they were fetched
  // by the client.
  void ClearPendingRemoteCommands();

  // Adds a remote command to the queue of pending remote commands.
  // Expected to be called by tests to set up the environment
  void AddPendingRemoteCommand(const em::RemoteCommand& command);

  // Stores an execution result of a remote command.
  // Intended to store command results when the server receives them from the
  // client.
  void AddRemoteCommandResult(const em::RemoteCommandResult& result);

  // Returns all pending remote commands.
  const std::vector<em::RemoteCommand>& GetPendingRemoteCommands() const;

  // Returns in |result| an execution result for a command with
  // a command ID == |id|.
  // If no result is available, returns false.
  // Expected to be called by tests to poll the remote command results.
  bool GetRemoteCommandResult(int id, em::RemoteCommandResult* result);

 private:
  // Maps a command ID to an execution result of that command on the client.
  base::flat_map<int, em::RemoteCommandResult> command_results;

  // Queue of pending remote commands.
  std::vector<em::RemoteCommand> pending_commands;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REMOTE_COMMANDS_STATE_H_
