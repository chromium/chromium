// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/directory_monitor.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class DirectoryMonitorTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  // Returns the test directory to monitor.
  base::FilePath GetDirectoryToMonitor() { return temp_dir_.GetPath(); }

  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
};

// Tests that the callback is not invoked when nothing happens.
TEST_F(DirectoryMonitorTest, NoNoise) {
  DirectoryMonitor monitor(GetDirectoryToMonitor());
  ::testing::StrictMock<base::MockRepeatingCallback<void(bool)>> callback;
  monitor.Start(callback.Get());
  task_environment_.RunUntilIdle();
}

// Tests that the callback is invoked when the directory is modified.
TEST_F(DirectoryMonitorTest, OneChange) {
  DirectoryMonitor monitor(GetDirectoryToMonitor());
  ::testing::StrictMock<base::MockRepeatingCallback<void(bool)>> callback;

  monitor.Start(callback.Get());
  task_environment_.RunUntilIdle();

  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(false)).WillOnce([&run_loop]() {
    run_loop.Quit();
  });
  ASSERT_TRUE(base::WriteFile(
      GetDirectoryToMonitor().Append(FILE_PATH_LITERAL("some file")),
      "hi, mom"));
  run_loop.Run();
}

// Tests that the callback is invoked multiple times for multiple
// modifications.
TEST_F(DirectoryMonitorTest, MultipleChanges) {
  DirectoryMonitor monitor(GetDirectoryToMonitor());
  ::testing::StrictMock<base::MockRepeatingCallback<void(bool)>> callback;

  monitor.Start(callback.Get());
  task_environment_.RunUntilIdle();
  {
    base::RunLoop run_loop;
    EXPECT_CALL(callback, Run(false)).WillOnce([&run_loop]() {
      run_loop.Quit();
    });
    ASSERT_TRUE(base::WriteFile(
        GetDirectoryToMonitor().Append(FILE_PATH_LITERAL("some file")),
        "hi, mom"));
    run_loop.Run();
    ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&callback));
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(callback, Run(false)).WillOnce([&run_loop]() {
      run_loop.Quit();
    });
    ASSERT_TRUE(base::WriteFile(
        GetDirectoryToMonitor().Append(FILE_PATH_LITERAL("other file")),
        "hi, dad"));
    run_loop.Run();
    ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&callback));
  }
}

// Tests that the callback is invoked when the directory's mtime is modified.
TEST_F(DirectoryMonitorTest, MtimeChange) {
  DirectoryMonitor monitor(GetDirectoryToMonitor());
  ::testing::StrictMock<base::MockRepeatingCallback<void(bool)>> callback;

  monitor.Start(callback.Get());
  task_environment_.RunUntilIdle();

  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(false)).WillOnce([&run_loop]() {
    run_loop.Quit();
  });
  const base::Time now = base::Time::Now();
  ASSERT_TRUE(base::TouchFile(GetDirectoryToMonitor(), now, now));
  run_loop.Run();
}
