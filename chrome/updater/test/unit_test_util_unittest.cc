// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/unit_test_util.h"

#include <optional>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test/test_scope.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/strings/string_number_conversions_win.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/test/test_executables.h"
#include "chrome/updater/win/test/test_strings.h"
#endif

namespace updater::test {
namespace {

std::string ToString(const std::string& s) {
  return s;
}

std::string ToString(const std::wstring& s) {
  return base::WideToUTF8(s);
}

}  // namespace

TEST(UnitTestUtil, ToString) {
  EXPECT_EQ(ToString("test"), ToString(L"test"));
}

TEST(UnitTestUtil, Processes) {
  auto print_processes_tester =
      [](const base::FilePath::StringType& process_name) {
        const std::string print_processes = PrintProcesses(process_name);
        const std::string regex_string = base::StringPrintf(
            R"(Found processes:\n)"
            R"(={72}\n(%s, pid=\d*, creation time=.*\n){2}={72}\n$)",
            ToString(process_name).c_str());
        bool is_match =
            re2::RE2::FullMatch(print_processes, re2::RE2(regex_string));
        if (!is_match) {
          ADD_FAILURE() << "regex:'" << regex_string << "'" << std::endl
                        << print_processes;
        }
        return is_match;
      };
#if BUILDFLAG(IS_WIN)
  // Ensure the test process is not running before the test.
  EXPECT_TRUE(KillProcesses(kTestProcessExecutableName, 0));
  EXPECT_TRUE(WaitForProcessesToExit(kTestProcessExecutableName,
                                     TestTimeouts::action_timeout()));
  EXPECT_FALSE(IsProcessRunning(kTestProcessExecutableName));

  // Start two long-lived processes and expect to find them running.
  std::vector<base::Process> long_running;
  long_running.push_back(
      LongRunningProcess(GetUpdaterScopeForTesting(), GetTestName(), nullptr));
  long_running.push_back(
      LongRunningProcess(GetUpdaterScopeForTesting(), GetTestName(), nullptr));
  for (const base::Process& p : long_running) {
    EXPECT_TRUE(p.IsValid());
  }
  EXPECT_TRUE(IsProcessRunning(kTestProcessExecutableName));

  EXPECT_TRUE(print_processes_tester(kTestProcessExecutableName));

  // Terminate the long-lived processes, expect to find them not running, then
  // inspect their exit code.
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
  base::FilePath::StringType unit_test = [] {
    base::FilePath unit_test_executable;
    base::PathService::Get(base::FILE_EXE, &unit_test_executable);
    return unit_test_executable.BaseName().value();
  }();
  EXPECT_TRUE(IsProcessRunning(unit_test));
  EXPECT_FALSE(WaitForProcessesToExit(unit_test, base::Milliseconds(1)));
  EXPECT_TRUE(print_processes_tester(unit_test));
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
  EXPECT_FALSE(DeleteFileAndEmptyParentDirectories(std::nullopt));

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

#if BUILDFLAG(IS_WIN)
TEST(UnitTestUtil, FindProcesses) {
  base::CommandLine command_line = GetTestProcessCommandLine(
      GetUpdaterScopeForTesting(), test::GetTestName());

  // Create a unique name for a shared event to be waited for in the test
  // process and signaled in this test.
  EventHolder event_holder(CreateWaitableEventForTest());

  command_line.AppendSwitchNative(kTestEventToWaitOn, event_holder.name);

  const base::Process process = base::LaunchProcess(command_line, {});
  ASSERT_TRUE(process.IsValid());

  EXPECT_TRUE(test::WaitFor([&] { return process.IsRunning(); }));
  EXPECT_EQ(test::FindProcesses(kTestProcessExecutableName).size(), 1U);

  event_holder.event.Signal();

  EXPECT_TRUE(test::WaitFor([&] { return !process.IsRunning(); }));
  EXPECT_TRUE(test::FindProcesses(kTestProcessExecutableName).empty());
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace updater::test
