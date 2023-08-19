// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REMOTE_COMMANDS_STATE_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REMOTE_COMMANDS_STATE_H_

#include <cstdint>

#include "base/containers/flat_map.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

// Stores information about current pending remote commands, and contains
// execution results of sent remote commands.
class RemoteCommandsState {
 public:
  // Interface for classes who would like to monitor remote command result.
  class Observer {
   public:
    // Called when a remote command result is available.
    virtual void OnRemoteCommandResultAvailable(int64_t command_id) = 0;

   protected:
    virtual ~Observer() = default;
  };

  RemoteCommandsState();
  RemoteCommandsState(RemoteCommandsState&& state) = delete;
  RemoteCommandsState& operator=(RemoteCommandsState&& state) = delete;
  ~RemoteCommandsState();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Removes all pending remote commands and available results.
  // Gets called on fake_dmserver construction.
  void ResetState();

  // Adds a remote command to the queue of pending remote commands.
  // Expected to be called by tests to set up the environment
  void AddPendingRemoteCommand(const em::RemoteCommand& command);

  // Stores an execution result of a remote command.
  // Intended to store command results when the server receives them from the
  // client.
  void AddRemoteCommandResult(const em::RemoteCommandResult& result);

  // Returns all pending remote commands and deletes them from the state.
  std::vector<em::RemoteCommand> ExtractPendingRemoteCommands();

  // Returns in |result| an execution result for a command with
  // a command ID == |id|.
  // If no result is available, returns false.
  // Expected to be called by tests to poll the remote command results.
  bool GetRemoteCommandResult(int64_t id, em::RemoteCommandResult* result);

  // Returns true if the remote command result is available.
  bool IsRemoteCommandResultAvailable(int64_t id);

 private:
  base::Lock lock_;

  // Maps a command ID to an execution result of that command on the client.
  base::flat_map<int64_t, em::RemoteCommandResult> command_results_
      GUARDED_BY(lock_);

  // Queue of pending remote commands.
  std::vector<em::RemoteCommand> pending_commands_ GUARDED_BY(lock_);

  scoped_refptr<base::ObserverListThreadSafe<Observer>> observer_list_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REMOTE_COMMANDS_STATE_H_
