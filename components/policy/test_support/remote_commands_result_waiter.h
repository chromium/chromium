// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REMOTE_COMMANDS_RESULT_WAITER_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REMOTE_COMMANDS_RESULT_WAITER_H_

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/remote_commands_state.h"

namespace policy {

// Blocks till the remote command result is available.
class RemoteCommandsResultWaiter : public RemoteCommandsState::Observer {
 public:
  RemoteCommandsResultWaiter(RemoteCommandsState* remote_commands_state,
                             int64_t command_id);

  ~RemoteCommandsResultWaiter() override;

  enterprise_management::RemoteCommandResult WaitAndGetResult();
  void WaitAndGetAck();

 private:
  void OnRemoteCommandResultAvailable(int64_t command_id) override;
  void OnRemoteCommandAcked(int64_t command_id) override;

  void WaitForResult();
  void WaitForAck();

  const raw_ptr<RemoteCommandsState> remote_commands_state_;
  const int64_t command_id_;
  base::RunLoop result_run_loop_, ack_run_loop_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REMOTE_COMMANDS_RESULT_WAITER_H_
