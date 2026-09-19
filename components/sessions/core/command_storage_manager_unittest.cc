// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/command_storage_manager.h"

#include <memory>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
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
#include "components/sessions/core/session_constants.h"
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

  // Returns a list of the files in the encrypted sessions directory, sorted
  // in chronological order (oldest first).
  std::vector<base::FilePath> GetEncryptedSessionFiles() {
    base::FilePath encrypted_dir = path_.Append(kEncryptedSessionsDirectory);
    std::vector<base::FilePath> files;
    base::FileEnumerator file_enum(encrypted_dir, false,
                                   base::FileEnumerator::FILES);
    for (base::FilePath name = file_enum.Next(); !name.empty();
         name = file_enum.Next()) {
      files.push_back(name);
    }
    std::sort(files.begin(), files.end());
    return files;
  }

  // Returns a list of the files in the cleartext sessions directory, sorted
  // in chronological order (oldest first).
  std::vector<base::FilePath> GetCleartextSessionFiles() {
    base::FilePath sessions_dir = path_.Append(kSessionsDirectory);
    std::vector<base::FilePath> files;
    base::FileEnumerator file_enum(sessions_dir, false,
                                   base::FileEnumerator::FILES);
    for (base::FilePath name = file_enum.Next(); !name.empty();
         name = file_enum.Next()) {
      files.push_back(name);
    }
    std::sort(files.begin(), files.end());
    return files;
  }
  base::FilePath path_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  base::test::ScopedFeatureList scoped_feature_list_;

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

// A delegate that rebuilds all of its commands when a write error is reported,
// which is what SessionService does. See
// SessionService::OnErrorWritingSessionCommands() and
// SessionService::ScheduleResetCommands().
class RebuildOnErrorDelegate : public TestCommandStorageManagerDelegate {
 public:
  ~RebuildOnErrorDelegate() override = default;

  // Must be called before any error can be reported. Not passed to the
  // constructor because the manager needs the delegate at construction time.
  void set_manager(CommandStorageManager* manager) { manager_ = manager; }

  // TestCommandStorageManagerDelegate:
  void OnErrorWritingSessionCommands() override {
    TestCommandStorageManagerDelegate::OnErrorWritingSessionCommands();
    manager_->set_pending_reset(true);
    manager_->ClearPendingCommands();
    manager_->AppendRebuildCommand(std::make_unique<SessionCommand>(201, 0));
  }

 private:
  raw_ptr<CommandStorageManager> manager_ = nullptr;
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

TEST_P(CommandStorageManagerTest,
       GetLastSessionCommandsBeforeEncryptorIsReady) {
  base::HistogramTester histogram_tester;
  TestCommandStorageManagerDelegate delegate;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt;
  os_crypt = os_crypt_async::GetTestOSCryptAsyncForTesting(
      /*is_sync_for_unittests=*/false);
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt.get(), backend_task_runner_);
  CommandStorageManagerTestHelper test_helper(&manager);

  // GetLastSessionCommands is called before the encryptor is ready.
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

  EncryptSessionStorageStage stage = GetEncryptSessionStorageStage();
  if (!GetParam().encryption_enabled ||
      stage == EncryptSessionStorageStage::kClearOnly) {
    EXPECT_FALSE(error);
    EXPECT_TRUE(commands.empty());
    histogram_tester.ExpectTotalCount(
        "Session.CommandStorageManager.EncryptedBackendUninitialized", 0);
  } else if (stage == EncryptSessionStorageStage::kWriteBothReadOnlyClear ||
             stage ==
                 EncryptSessionStorageStage::kWriteBothReadPreferEncrypted ||
             stage == EncryptSessionStorageStage::
                          kWriteEncryptedReadPreferEncrypted) {
    EXPECT_FALSE(error);
    EXPECT_TRUE(commands.empty());
    // SessionEncryptedBackendUninitialized::kGetLastSessionCommands = 3
    const int kGetLastSessionCommands = 3;
    histogram_tester.ExpectUniqueSample(
        "Session.CommandStorageManager.EncryptedBackendUninitialized",
        kGetLastSessionCommands, 1);
  } else {
    FAIL() << "Unhandled EncryptSessionStorageStage: "
           << static_cast<int>(stage);
  }
}

TEST_P(CommandStorageManagerTest, SaveBeforeEncryptorIsReady) {
  base::HistogramTester histogram_tester;
  TestCommandStorageManagerDelegate delegate;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt;
  os_crypt = os_crypt_async::GetTestOSCryptAsyncForTesting(
      /*is_sync_for_unittests=*/false);
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt.get(), backend_task_runner_);
  CommandStorageManagerTestHelper test_helper(&manager);

  // Save is called before the encryptor is ready.
  manager.AppendRebuildCommand(std::make_unique<SessionCommand>(101, 0));
  manager.Save();
  // The first call lets the encryptor become ready; the second lets any write
  // that was deferred until then complete.
  test_helper.RunMessageLoopUntilBackendDone();
  test_helper.RunMessageLoopUntilBackendDone();

  EncryptSessionStorageStage stage = GetEncryptSessionStorageStage();
  if (!GetParam().encryption_enabled ||
      stage == EncryptSessionStorageStage::kClearOnly) {
    EXPECT_EQ(0, delegate.error_count());
    EXPECT_TRUE(manager.pending_commands().empty());
    histogram_tester.ExpectTotalCount(
        "Session.CommandStorageManager.EncryptedBackendUninitialized", 0);
  } else if (stage == EncryptSessionStorageStage::kWriteBothReadOnlyClear) {
    EXPECT_EQ(0, delegate.error_count());
    EXPECT_TRUE(manager.pending_commands().empty());
    // SessionEncryptedBackendUninitialized::kSave = 0
    const int kSave = 0;
    histogram_tester.ExpectUniqueSample(
        "Session.CommandStorageManager.EncryptedBackendUninitialized", kSave,
        1);
  } else if (stage ==
                 EncryptSessionStorageStage::kWriteBothReadPreferEncrypted ||
             stage == EncryptSessionStorageStage::
                          kWriteEncryptedReadPreferEncrypted) {
    // The write is deferred until the encryptor is ready. Waiting for the
    // encryptor is not a write failure, so no error should be reported.
    EXPECT_EQ(0, delegate.error_count());
    EXPECT_TRUE(manager.pending_commands().empty());
    // SessionEncryptedBackendUninitialized::kSave = 0
    const int kSave = 0;
    histogram_tester.ExpectUniqueSample(
        "Session.CommandStorageManager.EncryptedBackendUninitialized", kSave,
        1);
  } else {
    FAIL() << "Unhandled EncryptSessionStorageStage: "
           << static_cast<int>(stage);
  }
  EXPECT_FALSE(manager.pending_reset());
  EXPECT_EQ(0, manager.commands_since_reset());
}

// Regression test for crbug.com/562992858. When a save is attempted before the
// encryptor is ready, the manager must not be left holding pending commands
// that are unaccounted for by `commands_since_reset()`. Otherwise a delegate
// that rebuilds its commands in response to a write error (as SessionService
// does) trips the CHECK in ClearPendingCommands().
TEST_P(CommandStorageManagerTest, RebuildAfterSaveBeforeEncryptorIsReady) {
  RebuildOnErrorDelegate delegate;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt =
      os_crypt_async::GetTestOSCryptAsyncForTesting(
          /*is_sync_for_unittests=*/false);
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt.get(), backend_task_runner_);
  delegate.set_manager(&manager);
  CommandStorageManagerTestHelper test_helper(&manager);

  // Save is called before the encryptor is ready.
  manager.AppendRebuildCommand(std::make_unique<SessionCommand>(101, 0));
  manager.Save();

  // Every pending command must still be counted by `commands_since_reset()`.
  EXPECT_GE(manager.commands_since_reset(),
            static_cast<int>(manager.pending_commands().size()));

  // Lets the encryptor become ready, and runs the delegate's error handler if
  // the manager reported an error.
  test_helper.RunMessageLoopUntilBackendDone();

  EXPECT_EQ(0, delegate.error_count());
  EXPECT_GE(manager.commands_since_reset(),
            static_cast<int>(manager.pending_commands().size()));

  // The deferred commands are written once the encryptor is available.
  test_helper.RunMessageLoopUntilBackendDone();
  if (test_helper.ShouldWriteCleartextFiles()) {
    EXPECT_FALSE(GetCleartextSessionFiles().empty());
  }
  if (test_helper.ShouldWriteEncryptedFiles()) {
    EXPECT_FALSE(GetEncryptedSessionFiles().empty());
  }
}

TEST_P(CommandStorageManagerTest,
       MoveCurrentSessionToLastSessionBeforeEncryptorIsReady) {
  base::HistogramTester histogram_tester;
  TestCommandStorageManagerDelegate delegate;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt;
  os_crypt = os_crypt_async::GetTestOSCryptAsyncForTesting(
      /*is_sync_for_unittests=*/false);
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt.get(), backend_task_runner_);
  CommandStorageManagerTestHelper test_helper(&manager);

  manager.MoveCurrentSessionToLastSession();
  test_helper.RunMessageLoopUntilBackendDone();

  EncryptSessionStorageStage stage = GetEncryptSessionStorageStage();
  if (!GetParam().encryption_enabled ||
      stage == EncryptSessionStorageStage::kClearOnly) {
    histogram_tester.ExpectTotalCount(
        "Session.CommandStorageManager.EncryptedBackendUninitialized", 0);
  } else if (stage == EncryptSessionStorageStage::kWriteBothReadOnlyClear ||
             stage ==
                 EncryptSessionStorageStage::kWriteBothReadPreferEncrypted ||
             stage == EncryptSessionStorageStage::
                          kWriteEncryptedReadPreferEncrypted) {
    // SessionEncryptedBackendUninitialized::kMoveCurrentSessionToLastSession
    const int kMoveCurrentSessionToLastSession = 1;
    histogram_tester.ExpectUniqueSample(
        "Session.CommandStorageManager.EncryptedBackendUninitialized",
        kMoveCurrentSessionToLastSession, 1);
  } else {
    FAIL() << "Unhandled EncryptSessionStorageStage: "
           << static_cast<int>(stage);
  }
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

TEST_P(CommandStorageManagerTest, EncryptedReadMatch) {
  base::HistogramTester histogram_tester;
  TestCommandStorageManagerDelegate delegate;
  // Setup by writing commands to the backend.
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
  CommandStorageManagerTestHelper test_helper(&manager);
  // Test only when both backends are written to and both are read.
  if (!test_helper.ShouldWriteEncryptedFiles() ||
      !test_helper.ShouldWriteCleartextFiles()) {
    GTEST_SKIP() << "Skipping test for non-encryption-related test params.";
  }
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
  EXPECT_EQ(2U, commands.size());

  // There should be a single record indicating a match.
  const int kMatch = 0;  // SessionReadComparisonResult::kMatch
  histogram_tester.ExpectUniqueSample(
      "Session.CommandStorageManager.EncryptedReadMatch", kMatch, 1);
}

TEST_P(CommandStorageManagerTest, EncryptedReadErrorWithBadEncryptedFile) {
  base::HistogramTester histogram_tester;
  TestCommandStorageManagerDelegate delegate;
  // Setup by writing commands to the backend.
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
  CommandStorageManagerTestHelper test_helper(&manager);
  if (!test_helper.ShouldWriteEncryptedFiles() ||
      !test_helper.ShouldWriteCleartextFiles()) {
    GTEST_SKIP() << "Skipping test because test params do not write both "
                    "cleartext and encrypted files.";
  }
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(101, 0)});
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(102, 0)});
  manager.Save();
  test_helper.RunMessageLoopUntilBackendDone();

  manager.MoveCurrentSessionToLastSession();
  test_helper.RunMessageLoopUntilBackendDone();

  // Corrupt the encrypted last session file.
  // Note that the cleartext version of this file remains intact.
  base::FilePath encrypted_dir = path_.Append(kEncryptedSessionsDirectory);
  base::FileEnumerator file_enum(encrypted_dir, false,
                                 base::FileEnumerator::FILES);
  std::vector<base::FilePath> files;
  for (base::FilePath name = file_enum.Next(); !name.empty();
       name = file_enum.Next()) {
    files.push_back(name);
  }
  std::sort(files.begin(), files.end());
  ASSERT_EQ(2U, files.size());
  base::FilePath last_session_file = files[0];
  ASSERT_TRUE(base::WriteFile(last_session_file, "garbage data"));

  // Use the CommandStorageManager to read the commands.
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

  // The read from should still succeed because the cleartext file is intact.
  EXPECT_FALSE(error);
  EXPECT_EQ(2U, commands.size());

  // There should be a single record indicating a mismatch due to an error.
  const int kErrorMismatch = 1;  // SessionReadComparisonResult::kErrorMismatch
  histogram_tester.ExpectUniqueSample(
      "Session.CommandStorageManager.EncryptedReadMatch", kErrorMismatch, 1);
}

TEST_P(CommandStorageManagerTest, EncryptedReadErrorWithMissingEncryptedFile) {
  base::HistogramTester histogram_tester;
  TestCommandStorageManagerDelegate delegate;
  // Setup by writing commands to the backend.
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
  CommandStorageManagerTestHelper test_helper(&manager);
  if (!test_helper.ShouldWriteEncryptedFiles() ||
      !test_helper.ShouldWriteCleartextFiles()) {
    GTEST_SKIP() << "Skipping test because test params do not write both "
                    "cleartext and encrypted files.";
  }
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(101, 0)});
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(102, 0)});
  manager.Save();
  test_helper.RunMessageLoopUntilBackendDone();

  manager.MoveCurrentSessionToLastSession();
  test_helper.RunMessageLoopUntilBackendDone();

  // Delete the encrypted last session file so it's missing.
  // We only delete `files[0]` because `MoveCurrentSessionToLastSession()`
  // opens a new current session file (`files[1]`) which is still held open.
  // On Windows, it's not possible to delete an actively open file.
  std::vector<base::FilePath> files = GetEncryptedSessionFiles();
  ASSERT_EQ(2U, files.size());
  ASSERT_TRUE(base::DeleteFile(files[0]));

  // Use the CommandStorageManager to read the commands.
  // `GetLastSessionCommands()` only attempts to read the last session
  // file (`files[0]` which is now missing); it does not attempt to read the
  // current session file (`files[1]`).
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

  // The read should still succeed because the cleartext file is intact.
  EXPECT_FALSE(error);
  EXPECT_EQ(2U, commands.size());

  // There should be a single record indicating a mismatch due to an error.
  const int kErrorMismatch = 1;  // SessionReadComparisonResult::kErrorMismatch
  histogram_tester.ExpectUniqueSample(
      "Session.CommandStorageManager.EncryptedReadMatch", kErrorMismatch, 1);
}

TEST_P(CommandStorageManagerTest, EncryptedReadCommandsMismatch) {
  base::HistogramTester histogram_tester;
  TestCommandStorageManagerDelegate delegate;
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
  CommandStorageManagerTestHelper test_helper(&manager);
  if (!test_helper.ShouldWriteEncryptedFiles() ||
      !test_helper.ShouldWriteCleartextFiles()) {
    GTEST_SKIP() << "Skipping test because test params do not write both "
                    "cleartext and encrypted files.";
  }

  // 1. Setup backup session with 1 command.
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(101, 0)});
  manager.Save();
  test_helper.RunMessageLoopUntilBackendDone();

  manager.MoveCurrentSessionToLastSession();
  test_helper.RunMessageLoopUntilBackendDone();

  std::vector<base::FilePath> files1 = GetEncryptedSessionFiles();
  ASSERT_EQ(2U, files1.size());
  base::FilePath last_session_file1 = files1[0];
  base::FilePath temp_backup;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(path_, &temp_backup));
  ASSERT_TRUE(base::CopyFile(last_session_file1, temp_backup));

  // 2. Setup a new session with 2 commands.
  manager.set_pending_reset(true);
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(101, 0)});
  manager.AppendRebuildCommand({std::make_unique<SessionCommand>(102, 0)});
  manager.Save();
  test_helper.RunMessageLoopUntilBackendDone();

  manager.MoveCurrentSessionToLastSession();
  test_helper.RunMessageLoopUntilBackendDone();

  std::vector<base::FilePath> files2 = GetEncryptedSessionFiles();
  ASSERT_EQ(2U, files2.size());
  base::FilePath last_session_file2 = files2[0];

  // 3. Overwrite the new "last" session with the backup.
  ASSERT_TRUE(base::CopyFile(temp_backup, last_session_file2));

  // 4. Read commands.
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

  // The read should still succeed.
  EXPECT_FALSE(error);
  if (GetEncryptSessionStorageStage() ==
      EncryptSessionStorageStage::kWriteBothReadPreferEncrypted) {
    EXPECT_EQ(1U, commands.size());
  } else {
    EXPECT_EQ(2U, commands.size());
  }

  // There should be a single record indicating a commands mismatch.
  // SessionReadComparisonResult::kCommandsMismatch = 2
  const int kCommandsMismatch = 2;
  histogram_tester.ExpectUniqueSample(
      "Session.CommandStorageManager.EncryptedReadMatch", kCommandsMismatch, 1);
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

TEST_P(CommandStorageManagerTest, DeleteLastSessionBeforeEncryptorIsReady) {
  base::HistogramTester histogram_tester;
  TestCommandStorageManagerDelegate delegate;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt;
  os_crypt = os_crypt_async::GetTestOSCryptAsyncForTesting(
      /*is_sync_for_unittests=*/false);
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt.get(), backend_task_runner_);
  CommandStorageManagerTestHelper test_helper(&manager);

  manager.DeleteLastSession();
  test_helper.RunMessageLoopUntilBackendDone();

  EncryptSessionStorageStage stage = GetEncryptSessionStorageStage();
  if (!GetParam().encryption_enabled ||
      stage == EncryptSessionStorageStage::kClearOnly) {
    histogram_tester.ExpectTotalCount(
        "Session.CommandStorageManager.EncryptedBackendUninitialized", 0);
  } else if (stage == EncryptSessionStorageStage::kWriteBothReadOnlyClear ||
             stage ==
                 EncryptSessionStorageStage::kWriteBothReadPreferEncrypted ||
             stage == EncryptSessionStorageStage::
                          kWriteEncryptedReadPreferEncrypted) {
    // SessionReadComparisonResult::kDeleteLastSession =2
    const int kDeleteLastSession = 2;
    histogram_tester.ExpectUniqueSample(
        "Session.CommandStorageManager.EncryptedBackendUninitialized",
        kDeleteLastSession, 1);
  } else {
    FAIL() << "Unhandled EncryptSessionStorageStage: "
           << static_cast<int>(stage);
  }
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

TEST_P(CommandStorageManagerTest, FallbackToCleartextOnMissingEncryptedFile) {
  TestCommandStorageManagerDelegate delegate;
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
  CommandStorageManagerTestHelper test_helper(&manager);
  if (!test_helper.ShouldWriteEncryptedFiles()) {
    return;
  }

  // Setup by writing commands to both cleartext and encrypted backends.
  {
    // Perform setup with a manager that writes to both backends.
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kEncryptSessionStorage,
        {{"stage", kEncryptSessionStorageStageWriteBothReadOnlyClear}});
    {
      CommandStorageManager setup_mgr(GetParam().session_type, path_, &delegate,
                                      os_crypt_async_.get(),
                                      backend_task_runner_);
      CommandStorageManagerTestHelper setup_helper(&setup_mgr);
      setup_mgr.AppendRebuildCommand(
          {std::make_unique<SessionCommand>(101, 0)});
      setup_mgr.AppendRebuildCommand(
          {std::make_unique<SessionCommand>(102, 0)});
      setup_mgr.Save();
      setup_helper.RunMessageLoopUntilBackendDone();
    }
    // Wait for the backend destructors of setup_mgr to finish, closing the
    // files, before scoped_feature_list is destroyed.
    test_helper.RunMessageLoopUntilBackendDone();
  }

  // Delete the encrypted files to simulate a missing file.
  base::DeletePathRecursively(path_.Append(kEncryptedSessionsDirectory));

  // Read the commands from the backend using a new manager.
  std::vector<std::unique_ptr<SessionCommand>> commands;
  bool error = false;
  bool finished = false;
  manager.GetLastSessionCommands(base::BindLambdaForTesting(
      [&commands, &error, &finished](
          std::vector<std::unique_ptr<SessionCommand>> commands_out,
          bool error_out) {
        commands = std::move(commands_out);
        error = error_out;
        finished = true;
      }));
  // Do not use RunMessageLoopUntilBackendDone() here, because we have 2 read
  // operations: the encrypted read that fails, and the fallback read of the
  // cleartext that succeeds.
  EXPECT_TRUE(base::test::RunUntil([&]() { return finished; }));

  // Fallback to cleartext should be successful.
  EXPECT_FALSE(error);
  ASSERT_EQ(2U, commands.size());
  EXPECT_EQ(101U, commands[0]->id());
  EXPECT_EQ(102U, commands[1]->id());
}

TEST_P(CommandStorageManagerTest, FallbackToCleartextOnCorruptedEncryptedFile) {
  TestCommandStorageManagerDelegate delegate;
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
  CommandStorageManagerTestHelper test_helper(&manager);
  if (!test_helper.ShouldWriteEncryptedFiles()) {
    return;
  }

  // Setup by writing commands to both cleartext and encrypted backends.
  {
    // Perform setup with a manager that writes to both backends.
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kEncryptSessionStorage,
        {{"stage", kEncryptSessionStorageStageWriteBothReadOnlyClear}});
    {
      CommandStorageManager setup_mgr(GetParam().session_type, path_, &delegate,
                                      os_crypt_async_.get(),
                                      backend_task_runner_);
      CommandStorageManagerTestHelper setup_helper(&setup_mgr);
      setup_mgr.AppendRebuildCommand(
          {std::make_unique<SessionCommand>(101, 0)});
      setup_mgr.AppendRebuildCommand(
          {std::make_unique<SessionCommand>(102, 0)});
      setup_mgr.Save();
      setup_helper.RunMessageLoopUntilBackendDone();
    }
    // Wait for the backend destructors of setup_mgr to finish, closing the
    // files, before scoped_feature_list is destroyed.
    test_helper.RunMessageLoopUntilBackendDone();
  }

  // Corrupt the encrypted session files to trigger a read error.
  base::FileEnumerator file_enum(path_.Append(kEncryptedSessionsDirectory),
                                 false, base::FileEnumerator::FILES);
  for (base::FilePath name = file_enum.Next(); !name.empty();
       name = file_enum.Next()) {
    base::File file(name, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
    ASSERT_TRUE(file.IsValid());
    file.SetLength(4);  // A truncated/corrupted header.
  }

  // Read the commands from the backend using a new manager.
  CommandStorageManager manager2(GetParam().session_type, path_, &delegate,
                                 os_crypt_async_.get(), backend_task_runner_);
  CommandStorageManagerTestHelper test_helper2(&manager2);
  std::vector<std::unique_ptr<SessionCommand>> commands;
  bool error = false;
  bool finished = false;
  manager2.GetLastSessionCommands(base::BindLambdaForTesting(
      [&commands, &error, &finished](
          std::vector<std::unique_ptr<SessionCommand>> commands_out,
          bool error_out) {
        commands = std::move(commands_out);
        error = error_out;
        finished = true;
      }));
  // Do not use RunMessageLoopUntilBackendDone() here, because we have 2 read
  // operations: the encrypted read that fails, and the fallback read of the
  // cleartext that succeeds.
  EXPECT_TRUE(base::test::RunUntil([&]() { return finished; }));

  // Fallback to cleartext should be successful.
  EXPECT_FALSE(error);
  ASSERT_EQ(2U, commands.size());
  EXPECT_EQ(101U, commands[0]->id());
  EXPECT_EQ(102U, commands[1]->id());
}

TEST_P(CommandStorageManagerTest, FallbackToCleartextFailsWithNoCleartextFile) {
  TestCommandStorageManagerDelegate delegate;
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
  CommandStorageManagerTestHelper test_helper(&manager);
  if (!test_helper.ShouldWriteEncryptedFiles()) {
    GTEST_SKIP() << "Skipping test because test params do not read from "
                    "encrypted files.";
  }

  // Setup by writing corrupted commands to the encrypted backend only.
  // Do not write to the cleartext backend.
  {
    // Perform setup with a manager that writes to only the encrypted backend.
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kEncryptSessionStorage,
        {{"stage",
          kEncryptSessionStorageStageWriteEncryptedReadPreferEncrypted}});
    {
      CommandStorageManager setup_mgr(GetParam().session_type, path_, &delegate,
                                      os_crypt_async_.get(),
                                      backend_task_runner_);
      CommandStorageManagerTestHelper setup_helper(&setup_mgr);
      setup_mgr.AppendRebuildCommand(
          {std::make_unique<SessionCommand>(101, 0)});
      setup_mgr.AppendRebuildCommand(
          {std::make_unique<SessionCommand>(102, 0)});
      setup_mgr.Save();
      setup_helper.RunMessageLoopUntilBackendDone();
    }
    // Wait for the backend destructors of setup_mgr to finish, closing the
    // files, before scoped_feature_list is destroyed.
    test_helper.RunMessageLoopUntilBackendDone();
  }

  // Corrupt the encrypted session file to trigger a read error.
  base::FileEnumerator file_enum(path_.Append(kEncryptedSessionsDirectory),
                                 false, base::FileEnumerator::FILES);
  for (base::FilePath name = file_enum.Next(); !name.empty();
       name = file_enum.Next()) {
    base::File file(name, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
    ASSERT_TRUE(file.IsValid());
    file.SetLength(4);  // A truncated/corrupted header.
  }
  std::vector<std::unique_ptr<SessionCommand>> commands;
  bool error = false;
  bool finished = false;
  manager.GetLastSessionCommands(base::BindLambdaForTesting(
      [&commands, &error, &finished](
          std::vector<std::unique_ptr<SessionCommand>> commands_out,
          bool error_out) {
        commands = std::move(commands_out);
        error = error_out;
        finished = true;
      }));
  // Do not use RunMessageLoopUntilBackendDone() here, because we have 2 read
  // operations: the encrypted read that fails, and the fallback read of the
  // cleartext that succeeds.
  EXPECT_TRUE(base::test::RunUntil([&]() { return finished; }));

  // Fallback fails.
  // In theory, this could be reported as an error, since the encrypted read
  // failed. In practice, it's simpler to return an empty list.  So we do not
  // have an error assertion here, just an empty list assertion.
  ASSERT_EQ(0U, commands.size());
}

TEST_P(CommandStorageManagerTest,
       CleartextFileDeletedInStageWriteEncryptedReadPreferEncrypted) {
  if (GetEncryptSessionStorageStage() !=
      EncryptSessionStorageStage::kWriteEncryptedReadPreferEncrypted) {
    return;
  }

  // Setup by writing commands to both cleartext and encrypted backends.
  // This simulates prior stage (WriteBothReadPreferEncrypted).
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kEncryptSessionStorage,
        {{"stage", kEncryptSessionStorageStageWriteBothReadPreferEncrypted}});
    {
      TestCommandStorageManagerDelegate setup_delegate;
      CommandStorageManager setup_mgr(GetParam().session_type, path_,
                                      &setup_delegate, os_crypt_async_.get(),
                                      backend_task_runner_);
      CommandStorageManagerTestHelper setup_helper(&setup_mgr);
      setup_mgr.AppendRebuildCommand(
          {std::make_unique<SessionCommand>(101, 0)});
      setup_mgr.AppendRebuildCommand(
          {std::make_unique<SessionCommand>(102, 0)});
      setup_mgr.Save();
      setup_helper.RunMessageLoopUntilBackendDone();
    }
  }
  EXPECT_FALSE(GetCleartextSessionFiles().empty());
  EXPECT_FALSE(GetEncryptedSessionFiles().empty());

  TestCommandStorageManagerDelegate delegate;
  CommandStorageManager manager(GetParam().session_type, path_, &delegate,
                                os_crypt_async_.get(), backend_task_runner_);
  CommandStorageManagerTestHelper test_helper(&manager);

  std::vector<std::unique_ptr<SessionCommand>> commands;
  bool error = false;
  bool finished = false;
  manager.GetLastSessionCommands(base::BindLambdaForTesting(
      [&commands, &error, &finished](
          std::vector<std::unique_ptr<SessionCommand>> commands_out,
          bool error_out) {
        commands = std::move(commands_out);
        error = error_out;
        finished = true;
      }));
  EXPECT_TRUE(base::test::RunUntil([&]() { return finished; }));
  test_helper.RunMessageLoopUntilBackendDone();

  EXPECT_FALSE(error);
  ASSERT_EQ(2U, commands.size());
  EXPECT_EQ(101U, commands[0]->id());
  EXPECT_EQ(102U, commands[1]->id());

  EXPECT_TRUE(GetCleartextSessionFiles().empty());
  EXPECT_FALSE(GetEncryptedSessionFiles().empty());
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
