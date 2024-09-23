// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/get_installed_version.h"

#include <stdio.h>
#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

using testing::_;

namespace {

constexpr char kChildModeSwitch[] =
    "get-installed-version-linux-unittest-child-mode";

enum class ChildMode {
  kNoVersion = 0,
  kProcessError = 1,
  kWithMonkey = 2,
  kWithVersion = 3,
};

ChildMode GetChildMode() {
  const auto switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          kChildModeSwitch);
  int mode_int = 0;
  base::StringToInt(switch_value, &mode_int);
  return static_cast<ChildMode>(mode_int);
}

}  // namespace

// A function run in a child process to print the desired version to stdout,
// just like Chrome does.
MULTIPROCESS_TEST_MAIN(GetProductVersionInChildProc) {
  switch (GetChildMode()) {
    case ChildMode::kNoVersion:
      // Return successful process exit without printing anything.
      return 0;

    case ChildMode::kProcessError:
      // Return a bad exit code without printing anything.
      break;

    case ChildMode::kWithMonkey:
      // Print something unexpected and report success.
      printf("monkey, monkey, monkey");
      return 0;

    case ChildMode::kWithVersion:
      // Print the current version and report success.
      printf("%s\n", version_info::GetVersionNumber().data());
      return 0;
  }
  return 1;
}

// A multi process test that exercises the Linux implementation of
// GetInstalledVersion.
class GetInstalledVersionLinuxTest : public ::testing::Test {
 protected:
  GetInstalledVersionLinuxTest()
      : original_command_line_(*base::CommandLine::ForCurrentProcess()) {}
  ~GetInstalledVersionLinuxTest() override {
    *base::CommandLine::ForCurrentProcess() = original_command_line_;
  }

  // Adds switches to the current process command line so that when
  // GetInstalledVersion relaunches the current process, it has the proper
  // child proc switch to lead to GetProductVersionInChildProc above and the
  // mode switch to tell the child how to behave.
  void AddChildCommandLineSwitches(ChildMode child_mode) {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kTestChildProcess,
                                    "GetProductVersionInChildProc");
    command_line->AppendSwitchASCII(
        kChildModeSwitch, base::NumberToString(static_cast<int>(child_mode)));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  // The original process command line; saved during construction and restored
  // during destruction.
  const base::CommandLine original_command_line_;
};

// Tests that an empty instance is returned when the child process reports
// nothing.
TEST_F(GetInstalledVersionLinuxTest, NoVersion) {
  AddChildCommandLineSwitches(ChildMode::kNoVersion);

  base::RunLoop run_loop;
  base::MockCallback<InstalledVersionCallback> callback;
  EXPECT_CALL(callback, Run(_))
      .WillOnce([&run_loop](InstalledAndCriticalVersion versions) {
        EXPECT_FALSE(versions.installed_version.IsValid());
        EXPECT_FALSE(versions.critical_version.has_value());
        run_loop.Quit();
      });
  GetInstalledVersion(callback.Get());
  run_loop.Run();
}

// Tests that an empty instance is returned when the child process exits with an
// error.
TEST_F(GetInstalledVersionLinuxTest, ProcessError) {
  AddChildCommandLineSwitches(ChildMode::kProcessError);

  base::RunLoop run_loop;
  base::MockCallback<InstalledVersionCallback> callback;
  EXPECT_CALL(callback, Run(_))
      .WillOnce([&run_loop](InstalledAndCriticalVersion versions) {
        ASSERT_FALSE(versions.installed_version.IsValid());
        EXPECT_FALSE(versions.critical_version.has_value());
        run_loop.Quit();
      });
  GetInstalledVersion(callback.Get());
  run_loop.Run();
}

// Tests that an empty instance is returned when the child process reports a
// monkey.
TEST_F(GetInstalledVersionLinuxTest, WithMonkey) {
  AddChildCommandLineSwitches(ChildMode::kWithMonkey);

  base::RunLoop run_loop;
  base::MockCallback<InstalledVersionCallback> callback;
  EXPECT_CALL(callback, Run(_))
      .WillOnce([&run_loop](InstalledAndCriticalVersion versions) {
        ASSERT_FALSE(versions.installed_version.IsValid());
        EXPECT_FALSE(versions.critical_version.has_value());
        run_loop.Quit();
      });
  GetInstalledVersion(callback.Get());
  run_loop.Run();
}

// Tests that the expected instance is returned when the child process reports a
// valid version.
// b/344455232: Disable as the test is failing on dbg build.
#if defined(NDEBUG)
#define MAYBE_WithVersion WithVersion
#else
#define MAYBE_WithVersion DISABLED_WithVersion
#endif
TEST_F(GetInstalledVersionLinuxTest, MAYBE_WithVersion) {
  AddChildCommandLineSwitches(ChildMode::kWithVersion);

  base::RunLoop run_loop;
  base::MockCallback<InstalledVersionCallback> callback;
  EXPECT_CALL(callback, Run(_))
      .WillOnce([&run_loop](InstalledAndCriticalVersion versions) {
        ASSERT_TRUE(versions.installed_version.IsValid());
        EXPECT_EQ(versions.installed_version, version_info::GetVersion());
        EXPECT_FALSE(versions.critical_version.has_value());
        run_loop.Quit();
      });
  GetInstalledVersion(callback.Get());
  run_loop.Run();
}
