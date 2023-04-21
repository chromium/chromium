// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/ash_browser_test_starter.h"
#include "ash/constants/ash_switches.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_switches.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

class AshBrowserTestStarterTest : public testing::Test {
 public:
  bool GetSummaryOutputFolder(base::FilePath& out_path) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    // Put lacros logs in CAS outputs on bots.
    if (!command_line->HasSwitch(switches::kTestLauncherSummaryOutput)) {
      return false;
    }
    out_path =
        command_line->GetSwitchValuePath(switches::kTestLauncherSummaryOutput)
            .DirName();
    return true;
  }
};

TEST_F(AshBrowserTestStarterTest, TestLacrosLogFolder) {
  base::FilePath summary_path;
  base::ScopedTempDir scoped_summary_dir;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool on_bot = GetSummaryOutputFolder(summary_path);
  if (!on_bot) {
    ASSERT_TRUE(scoped_summary_dir.CreateUniqueTempDir());
    command_line->AppendSwitchPath(
        switches::kTestLauncherSummaryOutput,
        scoped_summary_dir.GetPath().Append("output.json"));
    summary_path = scoped_summary_dir.GetPath();
  }
  command_line->AppendSwitchASCII(ash::switches::kLacrosChromePath, "/tmp/bbb");
  test::AshBrowserTestStarter starter;
  bool success = starter.PrepareEnvironmentForLacros();
  EXPECT_TRUE(success);
  EXPECT_TRUE(command_line->HasSwitch(switches::kUserDataDir));
  base::FilePath user_data_dir =
      command_line->GetSwitchValuePath(switches::kUserDataDir);
  base::FilePath expected_user_data_dir(
      summary_path.Append("AshBrowserTestStarterTest.TestLacrosLogFolder"));
  EXPECT_EQ(user_data_dir, expected_user_data_dir);
}

TEST_F(AshBrowserTestStarterTest, TestLacrosLogFolderWithOneRetry) {
  base::FilePath summary_path;
  base::ScopedTempDir scoped_summary_dir;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool on_bot = GetSummaryOutputFolder(summary_path);
  if (!on_bot) {
    ASSERT_TRUE(scoped_summary_dir.CreateUniqueTempDir());
    command_line->AppendSwitchPath(
        switches::kTestLauncherSummaryOutput,
        scoped_summary_dir.GetPath().Append("output.json"));
    summary_path = scoped_summary_dir.GetPath();
  }
  command_line->AppendSwitchASCII(ash::switches::kLacrosChromePath, "/tmp/bbb");
  ASSERT_TRUE(base::CreateDirectory(summary_path.Append(
      "AshBrowserTestStarterTest.TestLacrosLogFolderWithOneRetry")));
  test::AshBrowserTestStarter starter;
  bool success = starter.PrepareEnvironmentForLacros();
  EXPECT_TRUE(success);
  EXPECT_TRUE(command_line->HasSwitch(switches::kUserDataDir));
  base::FilePath user_data_dir =
      command_line->GetSwitchValuePath(switches::kUserDataDir);
  base::FilePath expected_user_data_dir(summary_path.Append(
      "AshBrowserTestStarterTest.TestLacrosLogFolderWithOneRetry.retry_1"));
  EXPECT_EQ(user_data_dir, expected_user_data_dir);
}

TEST_F(AshBrowserTestStarterTest, TestLacrosLogFolderWithTwoRetry) {
  base::FilePath summary_path;
  base::ScopedTempDir scoped_summary_dir;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool on_bot = GetSummaryOutputFolder(summary_path);
  if (!on_bot) {
    ASSERT_TRUE(scoped_summary_dir.CreateUniqueTempDir());
    command_line->AppendSwitchPath(
        switches::kTestLauncherSummaryOutput,
        scoped_summary_dir.GetPath().Append("output.json"));
    summary_path = scoped_summary_dir.GetPath();
  }
  command_line->AppendSwitchASCII(ash::switches::kLacrosChromePath, "/tmp/bbb");
  ASSERT_TRUE(base::CreateDirectory(summary_path.Append(
      "AshBrowserTestStarterTest.TestLacrosLogFolderWithTwoRetry")));
  ASSERT_TRUE(base::CreateDirectory(summary_path.Append(
      "AshBrowserTestStarterTest.TestLacrosLogFolderWithTwoRetry.retry_1")));
  test::AshBrowserTestStarter starter;
  bool success = starter.PrepareEnvironmentForLacros();
  EXPECT_TRUE(success);
  EXPECT_TRUE(command_line->HasSwitch(switches::kUserDataDir));
  base::FilePath user_data_dir =
      command_line->GetSwitchValuePath(switches::kUserDataDir);
  base::FilePath expected_user_data_dir(summary_path.Append(
      "AshBrowserTestStarterTest.TestLacrosLogFolderWithTwoRetry.retry_2"));
  EXPECT_EQ(user_data_dir, expected_user_data_dir);
}

TEST_F(AshBrowserTestStarterTest, TestLacrosLogFolderWithFiveRetry) {
  base::FilePath summary_path;
  base::ScopedTempDir scoped_summary_dir;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool on_bot = GetSummaryOutputFolder(summary_path);
  if (!on_bot) {
    ASSERT_TRUE(scoped_summary_dir.CreateUniqueTempDir());
    command_line->AppendSwitchPath(
        switches::kTestLauncherSummaryOutput,
        scoped_summary_dir.GetPath().Append("output.json"));
    summary_path = scoped_summary_dir.GetPath();
  }
  command_line->AppendSwitchASCII(ash::switches::kLacrosChromePath, "/tmp/bbb");
  ASSERT_TRUE(base::CreateDirectory(summary_path.Append(
      "AshBrowserTestStarterTest.TestLacrosLogFolderWithFiveRetry")));
  ASSERT_TRUE(base::CreateDirectory(summary_path.Append(
      "AshBrowserTestStarterTest.TestLacrosLogFolderWithFiveRetry.retry_1")));
  ASSERT_TRUE(base::CreateDirectory(summary_path.Append(
      "AshBrowserTestStarterTest.TestLacrosLogFolderWithFiveRetry.retry_2")));
  ASSERT_TRUE(base::CreateDirectory(summary_path.Append(
      "AshBrowserTestStarterTest.TestLacrosLogFolderWithFiveRetry.retry_3")));
  ASSERT_TRUE(base::CreateDirectory(summary_path.Append(
      "AshBrowserTestStarterTest.TestLacrosLogFolderWithFiveRetry.retry_4")));
  test::AshBrowserTestStarter starter;
  bool success = starter.PrepareEnvironmentForLacros();
  EXPECT_TRUE(success);
  EXPECT_FALSE(command_line->HasSwitch(switches::kUserDataDir));
}
