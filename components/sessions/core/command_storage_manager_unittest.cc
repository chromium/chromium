// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/command_storage_manager.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/sessions/core/command_storage_features.h"
#include "components/sessions/core/command_storage_manager_delegate.h"
#include "components/sessions/core/command_storage_manager_test_helper.h"
#include "components/sessions/core/session_command.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sessions {

using SessionType = CommandStorageManager::SessionType;
using internal::kEncryptSessionStorageStageWriteBothReadOnlyClear;
using internal::kEncryptSessionStorageStageWriteBothReadPreferEncrypted;
using internal::kEncryptSessionStorageStageWriteEncryptedReadPreferEncrypted;

struct TestParams {
  SessionType session_type;
  bool encryption_enabled;    // Enables feature kEncryptSessionStorage.
  const char* rollout_stage;  // Feature param kEncryptSessionStorage::stage.
};

class CommandStorageManagerTest : public testing::TestWithParam<TestParams> {
 protected:
  void SetUp() override {
    if (GetParam().encryption_enabled) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          kEncryptSessionStorage, {{"stage", GetParam().rollout_stage}});
    } else {
      scoped_feature_list_.InitAndDisableFeature(kEncryptSessionStorage);
    }
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    path_ = temp_dir_.GetPath();
    backend_task_runner_ =
        CommandStorageManager::CreateDefaultBackendTaskRunner();
    os_crypt_async_ = os_crypt_async::GetTestOSCryptAsyncForTesting(true);
  }

  base::FilePath path_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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

TEST_P(CommandStorageManagerTest, AppendCommandsAndSave) {
  TestCommandStorageManagerDelegate delegate;
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
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

TEST_P(CommandStorageManagerTest, ScheduleCommandsAndSave) {
  TestCommandStorageManagerDelegate delegate;
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
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

TEST_P(CommandStorageManagerTest, HasPendingSave) {
  TestCommandStorageManagerDelegate delegate;
  delegate.set_delayed_save(true);
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
  CommandStorageManagerTestHelper test_helper(&manager);
  EXPECT_FALSE(manager.HasPendingSave());

  manager.ScheduleCommand({std::make_unique<SessionCommand>(101, 0)});
  EXPECT_TRUE(manager.HasPendingSave());

  manager.Save();
  test_helper.RunMessageLoopUntilBackendDone();
  EXPECT_FALSE(manager.HasPendingSave());
}

TEST_P(CommandStorageManagerTest, GetLastSessionCommands) {
  TestCommandStorageManagerDelegate delegate;
  {  // Setup by writing commands to the backend.
    CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                  os_crypt_async_.get(), backend_task_runner_);
    CommandStorageManagerTestHelper test_helper(&manager);
    manager.AppendRebuildCommand({std::make_unique<SessionCommand>(101, 0)});
    manager.AppendRebuildCommand({std::make_unique<SessionCommand>(102, 0)});
    manager.Save();
    test_helper.RunMessageLoopUntilBackendDone();
  }

  // Read the commands from the backend (using a new manager).
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
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

TEST_P(CommandStorageManagerTest, OnErrorWritingSessionCommands) {
  TestCommandStorageManagerDelegate delegate;
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
  CommandStorageManagerTestHelper test_helper(&manager);
  // Wait for the encrypted backend to be initialized.
  test_helper.RunMessageLoopUntilBackendDone();
  test_helper.ForceAppendCommandsToFailForTesting();

  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(1, 0)});
  manager.Save();
  test_helper.RunMessageLoopUntilBackendDone();

  EXPECT_EQ(1, delegate.error_count());
}

TEST_P(CommandStorageManagerTest, MoveCurrentSessionToLastSession) {
  TestCommandStorageManagerDelegate delegate;
  // Setup by writing commands to the backend.
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
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

TEST_P(CommandStorageManagerTest, ClearPendingCommands) {
  TestCommandStorageManagerDelegate delegate;
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
  CommandStorageManagerTestHelper test_helper(&manager);
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(101, 0)});
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(102, 0)});

  manager.ClearPendingCommands();

  EXPECT_TRUE(manager.pending_reset());
  EXPECT_EQ(manager.commands_since_reset(), 0);
  EXPECT_TRUE(manager.pending_commands().empty());
  EXPECT_FALSE(manager.HasPendingSave());
}

TEST_P(CommandStorageManagerTest, EraseCommand) {
  TestCommandStorageManagerDelegate delegate;
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
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

TEST_P(CommandStorageManagerTest, SwapCommand) {
  TestCommandStorageManagerDelegate delegate;
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
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

TEST_P(CommandStorageManagerTest, SaveTwiceWithReset) {
  TestCommandStorageManagerDelegate delegate;
  // Setup by writing commands to the backend.
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
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

TEST_P(CommandStorageManagerTest, SaveTwiceWithoutReset) {
  TestCommandStorageManagerDelegate delegate;
  // Setup by writing commands to the backend.
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
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

TEST_P(CommandStorageManagerTest, ShouldWriteEncryptedFiles) {
  TestCommandStorageManagerDelegate delegate;
  std::string rollout_stage = GetParam().rollout_stage;
  SessionType session_type = GetParam().session_type;
  CommandStorageManager manager(session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
  CommandStorageManagerTestHelper test_helper(&manager);

  if (!GetParam().encryption_enabled) {
    EXPECT_FALSE(test_helper.ShouldWriteEncryptedFiles());
    return;
  }
  if (rollout_stage.empty()) {
    EXPECT_FALSE(test_helper.ShouldWriteEncryptedFiles());
    return;
  }
  if (rollout_stage == kEncryptSessionStorageStageWriteBothReadOnlyClear ||
      rollout_stage ==
          kEncryptSessionStorageStageWriteBothReadPreferEncrypted ||
      rollout_stage ==
          kEncryptSessionStorageStageWriteEncryptedReadPreferEncrypted) {
#if BUILDFLAG(IS_IOS)
    if (session_type == SessionType::kAppRestore ||
        session_type == SessionType::kSessionRestore) {
      // On iOS, SessionRestore and AppRestore do not use CommandStorageBackend.
      // As a practical matter, this scenario is not tested because it's not
      // included in kTestParams.  But we include it here for completeness.
      EXPECT_FALSE(test_helper.ShouldWriteEncryptedFiles());
      return;
    }
#endif
    EXPECT_TRUE(test_helper.ShouldWriteEncryptedFiles());
    return;
  }
  // Invalid rollout stage or some other unexpected case.
  EXPECT_FALSE(test_helper.ShouldWriteEncryptedFiles());
}

TEST_P(CommandStorageManagerTest, DeleteLastSession) {
  TestCommandStorageManagerDelegate delegate;
  // Setup by writing commands to the backend.
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
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

TEST_P(CommandStorageManagerTest, OnEncryptorReadyDurationRecorded) {
  base::HistogramTester histogram_tester;
  TestCommandStorageManagerDelegate delegate;
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
  if (CommandStorageManagerTestHelper(&manager).ShouldWriteEncryptedFiles()) {
    // Wait for OnEncryptorReady to be called.
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return !histogram_tester
                  .GetAllSamples(
                      "Session.CommandStorageManager.OnEncryptorReadyDuration")
                  .empty();
    }));
    histogram_tester.ExpectTotalCount(
        "Session.CommandStorageManager.OnEncryptorReadyDuration", 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Session.CommandStorageManager.OnEncryptorReadyDuration", 0);
  }
}

std::string TestParamNameGenerator(
    const testing::TestParamInfo<TestParams>& param_info) {
  std::string session_type_name;
  switch (param_info.param.session_type) {
    case SessionType::kAppRestore:
      session_type_name = "AppRestore";
      break;
    case SessionType::kSessionRestore:
      session_type_name = "SessionRestore";
      break;
    case SessionType::kTabRestore:
      session_type_name = "TabRestore";
      break;
  }
  std::string encryption_name;
  if (param_info.param.encryption_enabled) {
    std::string stage = param_info.param.rollout_stage;
    if (stage.empty()) {
      // Should be functionally identical to "Cleartext", but worth testing
      // separately to ensure the flag parsing is working correctly.
      encryption_name = "EncryptionStageEmpty";
    } else if (stage == kEncryptSessionStorageStageWriteBothReadOnlyClear) {
      encryption_name = "EncryptionStageWriteBothReadOnlyClear";
    } else if (stage ==
               kEncryptSessionStorageStageWriteBothReadPreferEncrypted) {
      encryption_name = "EncryptionStageWriteBothReadPreferEncrypted";
    } else if (stage ==
               kEncryptSessionStorageStageWriteEncryptedReadPreferEncrypted) {
      encryption_name = "EncryptionStageWriteEncryptedReadPreferEncrypted";
    } else {
      // Should be functionally identical to "Cleartext", but worth testing
      // separately to ensure the flag parsing is working correctly.
      encryption_name = "EncryptionStageInvalid";
    }
  } else {
    encryption_name = "Cleartext";
  }
  return base::JoinString({session_type_name, encryption_name}, "_");
}

const TestParams kTestParams[] = {
// On iOS, SessionRestore and AppRestore do not use CommandStorageBackend.
#if !BUILDFLAG(IS_IOS)
    {SessionType::kAppRestore, false, ""},
    {SessionType::kAppRestore, true, ""},
    {SessionType::kAppRestore, true,
     kEncryptSessionStorageStageWriteBothReadOnlyClear},
    {SessionType::kAppRestore, true,
     kEncryptSessionStorageStageWriteBothReadPreferEncrypted},
    {SessionType::kAppRestore, true,
     kEncryptSessionStorageStageWriteEncryptedReadPreferEncrypted},
    {SessionType::kAppRestore, true, "invalid_stage"},

    {SessionType::kSessionRestore, false, ""},
    {SessionType::kSessionRestore, true, ""},
    {SessionType::kSessionRestore, true,
     kEncryptSessionStorageStageWriteBothReadOnlyClear},
    {SessionType::kSessionRestore, true,
     kEncryptSessionStorageStageWriteBothReadPreferEncrypted},
    {SessionType::kSessionRestore, true,
     kEncryptSessionStorageStageWriteEncryptedReadPreferEncrypted},
    {SessionType::kSessionRestore, true, "invalid_stage"},
#endif  // !BUILDFLAG(IS_IOS)

    {SessionType::kTabRestore, false, ""},
    {SessionType::kTabRestore, true, ""},
    {SessionType::kTabRestore, true,
     kEncryptSessionStorageStageWriteBothReadOnlyClear},
    {SessionType::kTabRestore, true,
     kEncryptSessionStorageStageWriteBothReadPreferEncrypted},
    {SessionType::kTabRestore, true,
     kEncryptSessionStorageStageWriteEncryptedReadPreferEncrypted},
    {SessionType::kTabRestore, true, "invalid_stage"},
};

INSTANTIATE_TEST_SUITE_P(All,
                         CommandStorageManagerTest,
                         ::testing::ValuesIn(kTestParams),
                         TestParamNameGenerator);
}  // namespace sessions
