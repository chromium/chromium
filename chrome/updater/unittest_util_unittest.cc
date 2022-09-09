// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/unittest_util.h"

#include "base/logging.h"
#include "base/path_service.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace updater::test
