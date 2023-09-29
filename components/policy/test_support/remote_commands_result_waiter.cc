// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/remote_commands_result_waiter.h"

#include "base/run_loop.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/remote_commands_state.h"

namespace em = enterprise_management;

namespace policy {

RemoteCommandsResultWaiter::RemoteCommandsResultWaiter(
    RemoteCommandsState* remote_commands_state,
    int64_t command_id)
    : remote_commands_state_(remote_commands_state), command_id_(command_id) {
  remote_commands_state_->AddObserver(this);
}

RemoteCommandsResultWaiter::~RemoteCommandsResultWaiter() {
  remote_commands_state_->RemoveObserver(this);
}

void RemoteCommandsResultWaiter::WaitForResult() {
  em::RemoteCommandResult result;
  if (remote_commands_state_->GetRemoteCommandResult(command_id_, &result)) {
    // No need to wait, result is already available.
    return;
  }
  result_run_loop_.Run();
}

void RemoteCommandsResultWaiter::WaitForAck() {
  em::RemoteCommandResult result;
  if (remote_commands_state_->IsRemoteCommandAcked(command_id_)) {
    // No need to wait, the remote command was acknowledged.
    return;
  }
  ack_run_loop_.Run();
}

em::RemoteCommandResult RemoteCommandsResultWaiter::WaitAndGetResult() {
  WaitForResult();
  em::RemoteCommandResult result;
  const bool result_available =
      remote_commands_state_->GetRemoteCommandResult(command_id_, &result);
  // The result must be available now that the `result_run_loop_` has quit.
  CHECK(result_available);
  return result;
}

void RemoteCommandsResultWaiter::WaitAndGetAck() {
  WaitForAck();
  const bool result_available =
      remote_commands_state_->IsRemoteCommandAcked(command_id_);
  // The ack must be available now that the `ack_run_loop_` has quit.
  CHECK(result_available);
}

void RemoteCommandsResultWaiter::OnRemoteCommandResultAvailable(
    int64_t command_id) {
  if (command_id_ == command_id) {
    result_run_loop_.Quit();
  }
}

void RemoteCommandsResultWaiter::OnRemoteCommandAcked(int64_t command_id) {
  if (command_id_ == command_id) {
    ack_run_loop_.Quit();
  }
}

}  // namespace policy
