// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/command_storage_manager.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "components/sessions/core/command_storage_manager_delegate.h"
#include "components/sessions/core/command_storage_manager_test_helper.h"
#include "components/sessions/core/session_command.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sessions {

class CommandStorageManagerTest : public testing::Test {
 protected:
  // testing::TestWithParam:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    path_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Session"));
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

  // CommandStorageManagerDelegate:
  bool ShouldUseDelayedSave() override { return false; }

  void OnErrorWritingSessionCommands() override { ++error_count_; }

 private:
  int error_count_ = 0;
};

TEST_F(CommandStorageManagerTest, OnErrorWritingSessionCommands) {
  TestCommandStorageManagerDelegate delegate;
  CommandStorageManager manager(CommandStorageManager::kOther, path_,
                                &delegate);
  CommandStorageManagerTestHelper test_helper(&manager);
  manager.set_pending_reset(true);
  // Write a command, the delegate should not be notified of an error.
  manager.ScheduleCommand({std::make_unique<SessionCommand>(1, 0)});
  manager.Save();
  test_helper.RunMessageLoopUntilBackendDone();
  EXPECT_EQ(0, delegate.error_count());

  // Force the next write to fail, and verify the delegate is notified.
  test_helper.ForceAppendCommandsToFailForTesting();
  manager.ScheduleCommand({std::make_unique<SessionCommand>(1, 0)});
  manager.Save();
  test_helper.RunMessageLoopUntilBackendDone();
  EXPECT_EQ(1, delegate.error_count());
}

}  // namespace sessions
