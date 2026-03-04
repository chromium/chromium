// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/command_storage_manager.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/sessions/core/command_storage_manager_delegate.h"
#include "components/sessions/core/command_storage_manager_test_helper.h"
#include "components/sessions/core/session_command.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sessions {

using SessionType = CommandStorageManager::SessionType;

class CommandStorageManagerTest : public testing::Test {
 protected:
  // testing::TestWithParam:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    path_ = temp_dir_.GetPath();
  }

  base::FilePath path_;

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

class TestCommandStorageManagerDelegate : public CommandStorageManagerDelegate {
 public:
  ~TestCommandStorageManagerDelegate() override = default;

  int error_count() const { return error_count_; }
  void set_delayed_save(bool value) { delayed_save_ = value; }

  // CommandStorageManagerDelegate:
  bool ShouldUseDelayedSave() override { return delayed_save_; }

  void OnErrorWritingSessionCommands() override { ++error_count_; }

 private:
  int error_count_ = 0;
  bool delayed_save_ = false;
};

TEST_F(CommandStorageManagerTest, AppendCommandsAndSave) {
  TestCommandStorageManagerDelegate delegate;
  CommandStorageManager manager(SessionType::kSessionRestore, path_, &delegate);
  CommandStorageManagerTestHelper test_helper(&manager);
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(101, 0)});
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(102, 0)});
  EXPECT_TRUE(manager.pending_reset());
  EXPECT_EQ(manager.commands_since_reset(), 2);
  manager.Save();
  test_helper.RunMessageLoopUntilBackendDone();

  EXPECT_EQ(0, delegate.error_count());
  EXPECT_FALSE(manager.pending_reset());
  EXPECT_EQ(0, manager.commands_since_reset());
}

TEST_F(CommandStorageManagerTest, ScheduleCommandsAndSave) {
  TestCommandStorageManagerDelegate delegate;
  CommandStorageManager manager(SessionType::kSessionRestore, path_, &delegate);
  CommandStorageManagerTestHelper test_helper(&manager);
  manager.ScheduleCommand({std::make_unique<SessionCommand>(101, 0)});
  manager.ScheduleCommand({std::make_unique<SessionCommand>(102, 0)});
  EXPECT_TRUE(manager.pending_reset());
  EXPECT_EQ(2, manager.commands_since_reset());
  EXPECT_EQ(2U, manager.pending_commands().size());
  manager.Save();
  test_helper.RunMessageLoopUntilBackendDone();

  EXPECT_EQ(0, delegate.error_count());
  EXPECT_FALSE(manager.pending_reset());
  EXPECT_EQ(0, manager.commands_since_reset());
}

TEST_F(CommandStorageManagerTest, HasPendingSave) {
  TestCommandStorageManagerDelegate delegate;
  delegate.set_delayed_save(true);
  CommandStorageManager manager(SessionType::kSessionRestore, path_, &delegate);
  CommandStorageManagerTestHelper test_helper(&manager);
  EXPECT_FALSE(manager.HasPendingSave());

  manager.ScheduleCommand({std::make_unique<SessionCommand>(101, 0)});
  EXPECT_TRUE(manager.HasPendingSave());

  manager.Save();
  test_helper.RunMessageLoopUntilBackendDone();
  EXPECT_FALSE(manager.HasPendingSave());
}

TEST_F(CommandStorageManagerTest, GetLastSessionCommands) {
  TestCommandStorageManagerDelegate delegate;
  {  // Setup by writing commands to the backend.
    CommandStorageManager manager(SessionType::kSessionRestore, path_,
                                  &delegate);
    CommandStorageManagerTestHelper test_helper(&manager);
    manager.AppendRebuildCommand({std::make_unique<SessionCommand>(101, 0)});
    manager.AppendRebuildCommand({std::make_unique<SessionCommand>(102, 0)});
    manager.Save();
    test_helper.RunMessageLoopUntilBackendDone();
  }

  // Read the commands from the backend (using a new manager).
  CommandStorageManager manager(SessionType::kSessionRestore, path_, &delegate);
  CommandStorageManagerTestHelper test_helper(&manager);
  std::vector<std::unique_ptr<SessionCommand>> commands;
  bool error = false;
  manager.GetLastSessionCommands(base::BindLambdaForTesting(
      [&commands, &error](
          std::vector<std::unique_ptr<SessionCommand>> commands_out,
          bool error_out) {
        commands = std::move(commands_out);
        error = error_out;
      }));
  test_helper.RunMessageLoopUntilBackendDone();

  EXPECT_FALSE(error);
  ASSERT_EQ(2U, commands.size());
  EXPECT_EQ(101U, commands[0]->id());
  EXPECT_EQ(102U, commands[1]->id());
}

TEST_F(CommandStorageManagerTest, OnErrorWritingSessionCommands) {
  TestCommandStorageManagerDelegate delegate;
  CommandStorageManager manager(SessionType::kSessionRestore, path_, &delegate);
  CommandStorageManagerTestHelper test_helper(&manager);
  test_helper.ForceAppendCommandsToFailForTesting();

  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(1, 0)});
  manager.Save();
  test_helper.RunMessageLoopUntilBackendDone();

  EXPECT_EQ(1, delegate.error_count());
}

TEST_F(CommandStorageManagerTest, MoveCurrentSessionToLastSession) {
  TestCommandStorageManagerDelegate delegate;
  // Setup by writing commands to the backend.
  CommandStorageManager manager(SessionType::kSessionRestore, path_, &delegate);
  CommandStorageManagerTestHelper test_helper(&manager);
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(101, 0)});
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(102, 0)});
  manager.Save();
  test_helper.RunMessageLoopUntilBackendDone();

  manager.MoveCurrentSessionToLastSession();
  // Read the commands from the backend (using the SAME manager).
  std::vector<std::unique_ptr<SessionCommand>> commands;
  bool error = false;
  manager.GetLastSessionCommands(base::BindLambdaForTesting(
      [&commands, &error](
          std::vector<std::unique_ptr<SessionCommand>> commands_out,
          bool error_out) {
        commands = std::move(commands_out);
        error = error_out;
      }));
  test_helper.RunMessageLoopUntilBackendDone();

  EXPECT_FALSE(error);
  ASSERT_EQ(2U, commands.size());
  EXPECT_EQ(101U, commands[0]->id());
  EXPECT_EQ(102U, commands[1]->id());
}

TEST_F(CommandStorageManagerTest, ClearPendingCommands) {
  TestCommandStorageManagerDelegate delegate;
  CommandStorageManager manager(SessionType::kSessionRestore, path_, &delegate);
  CommandStorageManagerTestHelper test_helper(&manager);
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(101, 0)});
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(102, 0)});

  manager.ClearPendingCommands();

  EXPECT_TRUE(manager.pending_reset());
  EXPECT_EQ(manager.commands_since_reset(), 0);
  EXPECT_TRUE(manager.pending_commands().empty());
  EXPECT_FALSE(manager.HasPendingSave());
}

TEST_F(CommandStorageManagerTest, EraseCommand) {
  TestCommandStorageManagerDelegate delegate;
  CommandStorageManager manager(SessionType::kSessionRestore, path_, &delegate);
  CommandStorageManagerTestHelper test_helper(&manager);
  manager.ScheduleCommand(std::make_unique<SessionCommand>(101, 0));
  manager.ScheduleCommand(std::make_unique<SessionCommand>(102, 0));
  EXPECT_EQ(2U, manager.pending_commands().size());
  EXPECT_EQ(2, manager.commands_since_reset());

  SessionCommand* command1 = manager.pending_commands()[0].get();
  manager.EraseCommand(command1);

  EXPECT_EQ(1U, manager.pending_commands().size());
  EXPECT_EQ(1, manager.commands_since_reset());
  EXPECT_EQ(102U, manager.pending_commands()[0]->id());
}

TEST_F(CommandStorageManagerTest, SwapCommand) {
  TestCommandStorageManagerDelegate delegate;
  CommandStorageManager manager(SessionType::kSessionRestore, path_, &delegate);
  CommandStorageManagerTestHelper test_helper(&manager);
  manager.ScheduleCommand(std::make_unique<SessionCommand>(101, 0));
  manager.ScheduleCommand(std::make_unique<SessionCommand>(102, 0));
  EXPECT_EQ(2U, manager.pending_commands().size());
  EXPECT_EQ(2, manager.commands_since_reset());

  SessionCommand* command1 = manager.pending_commands()[0].get();
  auto new_command = std::make_unique<SessionCommand>(103, 0);
  manager.SwapCommand(command1, std::move(new_command));

  EXPECT_EQ(2U, manager.pending_commands().size());
  EXPECT_EQ(2, manager.commands_since_reset());
  EXPECT_EQ(103U, manager.pending_commands()[0]->id());
  EXPECT_EQ(102U, manager.pending_commands()[1]->id());
}

TEST_F(CommandStorageManagerTest, SaveTwiceWithReset) {
  TestCommandStorageManagerDelegate delegate;
  // Setup by writing commands to the backend.
  CommandStorageManager manager(SessionType::kSessionRestore, path_, &delegate);
  CommandStorageManagerTestHelper test_helper(&manager);
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(101, 0)});
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(102, 0)});
  manager.Save();
  test_helper.RunMessageLoopUntilBackendDone();
  EXPECT_FALSE(manager.pending_reset());

  // Add another command and save.
  manager.set_pending_reset(true);
  manager.AppendRebuildCommand(std::make_unique<SessionCommand>(103, 0));
  manager.Save();
  test_helper.RunMessageLoopUntilBackendDone();
  EXPECT_EQ(0, delegate.error_count());
  EXPECT_FALSE(manager.pending_reset());
  EXPECT_EQ(0, manager.commands_since_reset());
  EXPECT_TRUE(manager.pending_commands().empty());

  // Read the commands to confirm that only the last command is saved.
  manager.MoveCurrentSessionToLastSession();
  std::vector<std::unique_ptr<SessionCommand>> commands;
  bool error = false;
  manager.GetLastSessionCommands(base::BindLambdaForTesting(
      [&commands, &error](
          std::vector<std::unique_ptr<SessionCommand>> commands_out,
          bool error_out) {
        commands = std::move(commands_out);
        error = error_out;
      }));
  test_helper.RunMessageLoopUntilBackendDone();
  EXPECT_FALSE(error);
  ASSERT_EQ(1U, commands.size());
  EXPECT_EQ(103U, commands[0]->id());
}

TEST_F(CommandStorageManagerTest, SaveTwiceWithoutReset) {
  TestCommandStorageManagerDelegate delegate;
  // Setup by writing commands to the backend.
  CommandStorageManager manager(SessionType::kSessionRestore, path_, &delegate);
  CommandStorageManagerTestHelper test_helper(&manager);
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(101, 0)});
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(102, 0)});
  manager.Save();
  test_helper.RunMessageLoopUntilBackendDone();
  EXPECT_FALSE(manager.pending_reset());

  // Add another command and save.
  // Note that this test does NOT call manager.set_pending_reset(true).
  manager.AppendRebuildCommand(std::make_unique<SessionCommand>(103, 0));
  manager.Save();
  test_helper.RunMessageLoopUntilBackendDone();
  EXPECT_EQ(0, delegate.error_count());
  EXPECT_FALSE(manager.pending_reset());
  EXPECT_EQ(1, manager.commands_since_reset());
  EXPECT_TRUE(manager.pending_commands().empty());

  // Read the commands to confirm that they were saved correctly.
  manager.MoveCurrentSessionToLastSession();
  std::vector<std::unique_ptr<SessionCommand>> commands;
  bool error = false;
  manager.GetLastSessionCommands(base::BindLambdaForTesting(
      [&commands, &error](
          std::vector<std::unique_ptr<SessionCommand>> commands_out,
          bool error_out) {
        commands = std::move(commands_out);
        error = error_out;
      }));
  test_helper.RunMessageLoopUntilBackendDone();
  EXPECT_FALSE(error);
  ASSERT_EQ(3U, commands.size());
  EXPECT_EQ(101U, commands[0]->id());
  EXPECT_EQ(102U, commands[1]->id());
  EXPECT_EQ(103U, commands[2]->id());
}

TEST_F(CommandStorageManagerTest, DeleteLastSession) {
  TestCommandStorageManagerDelegate delegate;
  // Setup by writing commands to the backend.
  CommandStorageManager manager(SessionType::kSessionRestore, path_, &delegate);
  CommandStorageManagerTestHelper test_helper(&manager);
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(101, 0)});
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(102, 0)});
  manager.Save();
  test_helper.RunMessageLoopUntilBackendDone();

  manager.DeleteLastSession();
  // Read the commands from the backend (using the SAME manager).
  std::vector<std::unique_ptr<SessionCommand>> commands;
  bool error = false;
  manager.GetLastSessionCommands(base::BindLambdaForTesting(
      [&commands, &error](
          std::vector<std::unique_ptr<SessionCommand>> commands_out,
          bool error_out) {
        commands = std::move(commands_out);
        error = error_out;
      }));
  test_helper.RunMessageLoopUntilBackendDone();

  EXPECT_FALSE(error);
  EXPECT_TRUE(commands.empty());
}

}  // namespace sessions
