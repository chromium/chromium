// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/launch_mode_recorder.h"

#include <optional>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kLaunchModeMetric[] = "Launch.Mode2";

struct PathKeyAndLaunchMode {
  int path_key;
  LaunchMode launch_mode;
};

}  // namespace

class LaunchModeRecorderTest : public testing::Test {
 protected:
  void ComputeLaunchModeAndVerify(const base::CommandLine& cmd_line,
                                  LaunchMode expected_mode) {
    base::RunLoop run_loop;
    base::MockCallback<base::OnceCallback<void(std::optional<LaunchMode>)>>
        mock_callback;
    ON_CALL(mock_callback, Run(testing::_))
        .WillByDefault(
            [&run_loop](std::optional<LaunchMode>) { run_loop.Quit(); });
    EXPECT_CALL(mock_callback, Run(std::optional<LaunchMode>(expected_mode)))
        .WillOnce(testing::DoDefault());
    ComputeLaunchMode(cmd_line, mock_callback.Get());
    run_loop.Run();
  }

 private:
  base::test::TaskEnvironment env_;
};

TEST_F(LaunchModeRecorderTest, NoMetric) {
  base::HistogramTester histogram_tester;
  base::OnceCallback<void(std::optional<LaunchMode>)> record_callback =
      GetRecordLaunchModeForTesting();
  std::move(record_callback).Run(std::nullopt);
  histogram_tester.ExpectTotalCount(kLaunchModeMetric, 0);
}

TEST_F(LaunchModeRecorderTest, NoneMetric) {
  base::HistogramTester histogram_tester;
  base::OnceCallback<void(std::optional<LaunchMode>)> record_callback =
      GetRecordLaunchModeForTesting();
  std::move(record_callback).Run(LaunchMode::kNone);
  histogram_tester.ExpectTotalCount(kLaunchModeMetric, 0);
}

TEST_F(LaunchModeRecorderTest, SimpleMetric) {
  base::HistogramTester histogram_tester;
  base::OnceCallback<void(std::optional<LaunchMode>)> record_callback =
      GetRecordLaunchModeForTesting();
  std::move(record_callback).Run(LaunchMode::kWithUrl);
  histogram_tester.ExpectUniqueSample(kLaunchModeMetric, LaunchMode::kWithUrl,
                                      1);
}

#if BUILDFLAG(IS_WIN)
TEST_F(LaunchModeRecorderTest, WinNotification) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  cmd_line.AppendSwitch(switches::kNotificationLaunchId);
  ComputeLaunchModeAndVerify(cmd_line, LaunchMode::kWinPlatformNotification);
}

TEST_F(LaunchModeRecorderTest, SlowModeChrome) {
  // Normal launch with a URL.
  base::CommandLine cmd_line_url(base::CommandLine::NO_PROGRAM);
  cmd_line_url.AppendArg("http://www.google.com");
  ComputeLaunchModeAndVerify(cmd_line_url, LaunchMode::kWithUrl);

  // Shell registration with a URL.
  base::CommandLine single_arg_cl = base::CommandLine::FromString(
      base::StrCat({L"program --single-argument ", L"http://www.google.com"}));
  ComputeLaunchModeAndVerify(single_arg_cl, LaunchMode::kProtocolHandler);

  // Normal launch with a file.
  base::CommandLine cmd_line_file(base::CommandLine::NO_PROGRAM);
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());
  base::FilePath test_file;
  base::CreateTemporaryFileInDir(test_dir.GetPath(), &test_file);
  ASSERT_TRUE(base::PathExists(test_file));
  cmd_line_file.AppendArgPath(test_file);
  ComputeLaunchModeAndVerify(cmd_line_file, LaunchMode::kWithFile);

  // Shell registration with a file.
  single_arg_cl = base::CommandLine::FromString(
      base::StrCat({L"program --single-argument ", test_file.value()}));
  ComputeLaunchModeAndVerify(single_arg_cl, LaunchMode::kFileTypeHandler);
}

TEST_F(LaunchModeRecorderTest, SlowModeWebApp) {
  // Web app with a url.
  base::CommandLine cmd_line_url(base::CommandLine::NO_PROGRAM);
  cmd_line_url.AppendSwitchASCII(switches::kAppId, "abcdefg");
  cmd_line_url.AppendArg("http://www.google.com");
  ComputeLaunchModeAndVerify(cmd_line_url, LaunchMode::kWebAppProtocolHandler);

  // Web app with a file.
  base::CommandLine cmd_line_file(base::CommandLine::NO_PROGRAM);
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());
  base::FilePath test_file;
  base::CreateTemporaryFileInDir(test_dir.GetPath(), &test_file);
  ASSERT_TRUE(base::PathExists(test_file));
  cmd_line_file.AppendSwitchASCII(switches::kAppId, "abcdefg");
  cmd_line_file.AppendArgPath(test_file);
  ComputeLaunchModeAndVerify(cmd_line_file, LaunchMode::kWebAppFileTypeHandler);
}

TEST_F(LaunchModeRecorderTest, SlowModeChromeShortcut) {
  static constexpr struct PathKeyAndLaunchMode kPathKeysAndModes[] = {
      {base::DIR_COMMON_START_MENU, LaunchMode::kShortcutStartMenu},
      {base::DIR_START_MENU, LaunchMode::kShortcutStartMenu},
      {base::DIR_COMMON_DESKTOP, LaunchMode::kShortcutDesktop},
      {base::DIR_USER_DESKTOP, LaunchMode::kShortcutDesktop},
      {base::DIR_TASKBAR_PINS, LaunchMode::kShortcutTaskbar},
  };
  for (const auto& path_key_and_mode : kPathKeysAndModes) {
    base::FilePath path;
    ASSERT_TRUE(base::PathService::Get(path_key_and_mode.path_key, &path));
    // Normal launch with a URL.
    base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
    cmd_line.AppendSwitchPath(switches::kSourceShortcut, path);
    ComputeLaunchModeAndVerify(cmd_line, path_key_and_mode.launch_mode);
  }
}

TEST_F(LaunchModeRecorderTest, SlowModeWebAppShortcut) {
  static constexpr struct PathKeyAndLaunchMode kPathKeysAndModes[] = {
      {base::DIR_COMMON_START_MENU, LaunchMode::kWebAppShortcutStartMenu},
      {base::DIR_START_MENU, LaunchMode::kWebAppShortcutStartMenu},
      {base::DIR_COMMON_DESKTOP, LaunchMode::kWebAppShortcutDesktop},
      {base::DIR_USER_DESKTOP, LaunchMode::kWebAppShortcutDesktop},
      {base::DIR_TASKBAR_PINS, LaunchMode::kWebAppShortcutTaskbar},
  };
  for (const auto& path_key_and_mode : kPathKeysAndModes) {
    base::FilePath path;
    ASSERT_TRUE(base::PathService::Get(path_key_and_mode.path_key, &path));
    // Normal launch with a URL.
    base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
    cmd_line.AppendSwitchPath(switches::kSourceShortcut, path);
    cmd_line.AppendSwitchASCII(switches::kAppId, "abcdefg");
    ComputeLaunchModeAndVerify(cmd_line, path_key_and_mode.launch_mode);
  }
}

class LaunchModeRecorderNoneTest
    : public LaunchModeRecorderTest,
      public ::testing::WithParamInterface<const char*> {
 public:
  LaunchModeRecorderNoneTest() = default;
  LaunchModeRecorderNoneTest(const LaunchModeRecorderNoneTest&) = delete;
  LaunchModeRecorderNoneTest& operator=(const LaunchModeRecorderNoneTest&) =
      delete;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LaunchModeRecorderNoneTest,
                         testing::Values(switches::kRestart,
                                         switches::kNoStartupWindow,
                                         switches::kUninstallAppId,
                                         switches::kListApps,
                                         switches::kInstallChromeApp,
                                         switches::kFromInstaller,
                                         switches::kUninstall));

// Test that we don't record a LaunchMode for a variety of switches.
TEST_P(LaunchModeRecorderNoneTest, NoLaunchMode) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  cmd_line.AppendSwitch(GetParam());
  ComputeLaunchModeAndVerify(cmd_line, LaunchMode::kNone);
}

#elif !BUILDFLAG(IS_MAC)

TEST_F(LaunchModeRecorderTest, Other) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  ComputeLaunchModeAndVerify(cmd_line, LaunchMode::kOtherOS);
}

#else  // IS_MAC

TEST_F(LaunchModeRecorderTest, Mac) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  ComputeLaunchModeAndVerify(cmd_line, LaunchMode::kMacUndockedDiskLaunch);
}

#endif  // BUILDFLAG(IS_WIN)
