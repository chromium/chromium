// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/unittest_util.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/updater/win/test/test_executables.h"
#endif

namespace updater::test {

TEST(UnitTestUtil, Processes) {
#if BUILDFLAG(IS_WIN)
  EXPECT_FALSE(IsProcessRunning(kTestProcessExecutableName));

  std::vector<base::Process> long_running;
  long_running.push_back(LongRunningProcess(nullptr));
  long_running.push_back(LongRunningProcess(nullptr));
  for (const base::Process& p : long_running) {
    EXPECT_TRUE(p.IsValid());
  }
  EXPECT_TRUE(IsProcessRunning(kTestProcessExecutableName));

  constexpr int kExitCode = 12345;
  EXPECT_FALSE(WaitForProcessesToExit(kTestProcessExecutableName,
                                      base::Milliseconds(1)));
  EXPECT_TRUE(KillProcesses(kTestProcessExecutableName, kExitCode));
  EXPECT_TRUE(WaitForProcessesToExit(kTestProcessExecutableName,
                                     TestTimeouts::action_timeout()));
  EXPECT_FALSE(IsProcessRunning(kTestProcessExecutableName));

  for (const base::Process& p : long_running) {
    int exit_code = 0;
    EXPECT_TRUE(
        p.WaitForExitWithTimeout(TestTimeouts::tiny_timeout(), &exit_code));
    EXPECT_EQ(exit_code, kExitCode);
  }
#else
  // Test the state of the process for the unit test process itself.
  base::FilePath::StringType unit_test = []() {
    base::FilePath unit_test_executable;
    base::PathService::Get(base::FILE_EXE, &unit_test_executable);
    return unit_test_executable.BaseName().value();
  }();
  EXPECT_TRUE(IsProcessRunning(unit_test));
  EXPECT_FALSE(WaitForProcessesToExit(unit_test, base::Milliseconds(1)));
#endif  // IS_WIN
}

TEST(UnitTestUtil, GetTestName) {
  EXPECT_EQ(GetTestName(), "UnitTestUtil.GetTestName");
}

// Enable the test to print the effective values for the test timeouts when
// debugging timeout issues.
TEST(UnitTestUtil, DISABLED_PrintTestTimeouts) {
  VLOG(0) << "action-timeout:"
          << TestTimeouts::action_timeout().InMilliseconds()
          << ", action-max-timeout:"
          << TestTimeouts::action_max_timeout().InMilliseconds()
          << ", test-launcher-timeout:"
          << TestTimeouts::test_launcher_timeout().InMilliseconds();
}

TEST(UnitTestUtil, DeleteFileAndEmptyParentDirectories) {
  EXPECT_FALSE(DeleteFileAndEmptyParentDirectories(absl::nullopt));

  const base::FilePath path_not_found(FILE_PATH_LITERAL("path-not-found"));
  EXPECT_TRUE(DeleteFileAndEmptyParentDirectories(path_not_found));

  // Create something in temp so that `DeleteFileAndEmptyParentDirectories()`
  // does not delete the temp directory of this process because it is empty.
  base::ScopedTempDir a_temp_dir;
  ASSERT_TRUE(a_temp_dir.CreateUniqueTempDir());

  base::FilePath temp_path;
  ASSERT_TRUE(GetTempDir(&temp_path));

  // Create and delete the following path "some_dir/dir_in_dir/file_in_dir".
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath dir_in_dir;
  EXPECT_TRUE(base::CreateTemporaryDirInDir(
      temp_dir.GetPath(), FILE_PATH_LITERAL("UnitTestUtil"), &dir_in_dir));
  base::FilePath file_in_dir;
  EXPECT_TRUE(CreateTemporaryFileInDir(dir_in_dir, &file_in_dir));
  EXPECT_TRUE(DeleteFileAndEmptyParentDirectories(file_in_dir));
  EXPECT_FALSE(base::DirectoryExists(temp_dir.GetPath()));
  EXPECT_TRUE(base::DirectoryExists(temp_path));
}

}  // namespace updater::test
