// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/unittest_util.h"

#include "base/path_service.h"
#include "base/test/test_timeouts.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/win/test/test_executables.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater::test {
namespace {

// Returns the name of the unit test program, which is this program.
base::FilePath::StringType GetUnitTestExecutableName() {
  base::FilePath unit_test_executable;
  base::PathService::Get(base::FILE_EXE, &unit_test_executable);
  return unit_test_executable.BaseName().value();
}

}  // namespace

TEST(UnitTestUtil, Processes) {
  // Test the state of the process for the unit test process itself.
  base::FilePath::StringType unit_test = GetUnitTestExecutableName();
  // TODO(crbug.com/1352190) - remove process name insertion after the flakiness
  // of the test is resolved.
  EXPECT_TRUE(IsProcessRunning(unit_test)) << unit_test;
  EXPECT_FALSE(WaitForProcessesToExit(unit_test, base::Milliseconds(1)));

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
#endif  // IS_WIN
}

}  // namespace updater::test
