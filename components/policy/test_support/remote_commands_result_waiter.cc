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

void RemoteCommandsResultWaiter::Wait() {
  em::RemoteCommandResult result;
  if (remote_commands_state_->GetRemoteCommandResult(command_id_, &result)) {
    // No need to wait, result is already available.
    return;
  }
  run_loop_.Run();
}

em::RemoteCommandResult RemoteCommandsResultWaiter::WaitAndGetResult() {
  Wait();
  em::RemoteCommandResult result;
  const bool result_available =
      remote_commands_state_->GetRemoteCommandResult(command_id_, &result);
  // The result must be available now that the `run_loop_` has quit.
  CHECK(result_available);
  return result;
}

void RemoteCommandsResultWaiter::OnRemoteCommandResultAvailable(
    int64_t command_id) {
  if (command_id_ == command_id) {
    run_loop_.Quit();
  }
}

}  // namespace policy
