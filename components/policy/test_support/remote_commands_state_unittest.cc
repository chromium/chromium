// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/remote_commands_state.h"

#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

TEST(RemoteCommandsStateTest, AddPendingRemoteCommand) {
  em::RemoteCommand command;
  command.set_command_id(1);
  command.set_type(em::RemoteCommand::DEVICE_REMOTE_POWERWASH);
  command.set_payload("{}");
  RemoteCommandsState state;

  state.AddPendingRemoteCommand(command);

  const std::vector<em::RemoteCommand> pending_commands =
      state.ExtractPendingRemoteCommands();
  EXPECT_EQ(pending_commands.size(), 1u);
  EXPECT_EQ(pending_commands[0].command_id(), 1);
  EXPECT_EQ(pending_commands[0].payload(), "{}");
  EXPECT_EQ(pending_commands[0].type(),
            em::RemoteCommand::DEVICE_REMOTE_POWERWASH);
}

TEST(RemoteCommandsStateTest, IsRemoteCommandAcked) {
  const int64_t command_id = 1;
  RemoteCommandsState state;

  state.AddRemoteCommandAcked(command_id);

  EXPECT_TRUE(state.IsRemoteCommandAcked(command_id));
}

TEST(RemoteCommandsStateTest, IsRemoteCommandNotAcked) {
  const int64_t command_id = 1;
  RemoteCommandsState state;

  EXPECT_FALSE(state.IsRemoteCommandAcked(command_id));
}

TEST(RemoteCommandsStateTest, GetUnavailableCommandResult) {
  RemoteCommandsState state;

  em::RemoteCommandResult command_result;
  EXPECT_FALSE(state.GetRemoteCommandResult(/*id=*/0, &command_result));
}

TEST(RemoteCommandsStateTest, GetAvailableCommandResult) {
  em::RemoteCommandResult result;
  result.set_command_id(10);
  result.set_payload("{}");
  RemoteCommandsState state;
  state.AddRemoteCommandResult(result);

  em::RemoteCommandResult command_result;
  bool ok = state.GetRemoteCommandResult(/*id=*/10, &command_result);

  ASSERT_TRUE(ok);
  EXPECT_EQ(result.SerializeAsString(), command_result.SerializeAsString());
}

TEST(RemoteCommandsStateTest, ResetsStateCorrectly) {
  em::RemoteCommandResult result;
  result.set_command_id(10);
  result.set_payload("{}");
  em::RemoteCommand command;
  command.set_command_id(1);
  command.set_type(em::RemoteCommand::DEVICE_REMOTE_POWERWASH);
  command.set_payload("{}");
  RemoteCommandsState state;
  state.AddPendingRemoteCommand(command);
  state.AddRemoteCommandResult(result);

  state.ResetState();

  const std::vector<em::RemoteCommand> pending_commands =
      state.ExtractPendingRemoteCommands();
  EXPECT_EQ(pending_commands.size(), 0u);
  EXPECT_FALSE(state.GetRemoteCommandResult(/*id=*/10, &result));
}

}  // namespace policy
