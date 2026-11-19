// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/remote_commands_state.h"

#include <vector>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

class MockRemoteCommandsStateObserver : public RemoteCommandsState::Observer {
 public:
  MOCK_METHOD(void, OnRemoteCommandResultAvailable, (int64_t), (override));
  MOCK_METHOD(void, OnRemoteCommandAcked, (int64_t), (override));
  MOCK_METHOD(void,
              OnRemoteCommandAdded,
              (int64_t, const em::RemoteCommand&, RemoteCommandScope),
              (override));
};

}  // namespace

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

TEST(RemoteCommandsStateTest, NotifiesObserverOnRemoteCommandAdded) {
  base::test::TaskEnvironment task_environment;
  RemoteCommandsState state;
  testing::StrictMock<MockRemoteCommandsStateObserver> observer;
  state.AddObserver(&observer);

  em::RemoteCommand command;
  command.set_type(em::RemoteCommand::DEVICE_REMOTE_POWERWASH);
  command.set_payload("{}");

  base::RunLoop run_loop;
  EXPECT_CALL(observer,
              OnRemoteCommandAdded(1, testing::_, RemoteCommandScope::kProfile))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  state.AddPendingRemoteCommand(command, RemoteCommandScope::kProfile);
  run_loop.Run();

  state.RemoveObserver(&observer);
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
  EXPECT_TRUE(state.GetIssuedCommands().empty());
}

TEST(RemoteCommandsStateTest, ExtractPendingRemoteCommandsByScope) {
  em::RemoteCommand command;
  command.set_type(em::RemoteCommand::COMMAND_ECHO_TEST);
  RemoteCommandsState state;
  int64_t any_id = state.AddPendingRemoteCommand(command);
  int64_t browser_id =
      state.AddPendingRemoteCommand(command, RemoteCommandScope::kBrowser);
  int64_t profile_id =
      state.AddPendingRemoteCommand(command, RemoteCommandScope::kProfile);

  std::vector<em::RemoteCommand> profile_commands =
      state.ExtractPendingRemoteCommands(RemoteCommandScope::kProfile);
  ASSERT_EQ(profile_commands.size(), 2u);
  EXPECT_EQ(profile_commands[0].command_id(), any_id);
  EXPECT_EQ(profile_commands[1].command_id(), profile_id);

  std::vector<em::RemoteCommand> browser_commands =
      state.ExtractPendingRemoteCommands(RemoteCommandScope::kBrowser);
  ASSERT_EQ(browser_commands.size(), 1u);
  EXPECT_EQ(browser_commands[0].command_id(), browser_id);
  EXPECT_TRUE(state.ExtractPendingRemoteCommands().empty());
}

}  // namespace policy
