// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/post_reboot_registration.h"

#include <windows.h>

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/test/test_branding.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

const char kSwitch1[] = "these_are_my";
const char kSwitch2[] = "switches.";

}  // namespace

TEST(PostRebootRegistrationTests, RegisterRunOnceOnRestart) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);

  PostRebootRegistration post_reboot(TEST_PRODUCT_SHORTNAME_STRING);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  std::string cleanup_id("test_cleanup_id");
  command_line.AppendSwitch(kSwitch1);
  command_line.AppendSwitch(kSwitch2);

  EXPECT_TRUE(post_reboot.RegisterRunOnceOnRestart(cleanup_id, command_line));

  // Start by manually validating the registry value.
  base::FilePath exe_path = PreFetchedPaths::GetInstance()->GetExecutablePath();
  command_line.SetProgram(exe_path);

  std::string switch_str(kPostRebootSwitchesInOtherRegistryKeySwitch);
  EXPECT_TRUE(RunOnceCommandLineContains(TEST_PRODUCT_SHORTNAME_STRING,
                                         base::UTF8ToWide(switch_str).c_str()));
  EXPECT_TRUE(RunOnceCommandLineContains(TEST_PRODUCT_SHORTNAME_STRING,
                                         base::UTF8ToWide(cleanup_id).c_str()));
  EXPECT_FALSE(RunOnceCommandLineContains(TEST_PRODUCT_SHORTNAME_STRING,
                                          base::UTF8ToWide(kSwitch1).c_str()));
  EXPECT_FALSE(RunOnceCommandLineContains(TEST_PRODUCT_SHORTNAME_STRING,
                                          base::UTF8ToWide(kSwitch2).c_str()));
  EXPECT_TRUE(RunOnceOverrideCommandLineContains(
      cleanup_id, base::UTF8ToWide(kSwitch1).c_str()));
  EXPECT_TRUE(RunOnceOverrideCommandLineContains(
      cleanup_id, base::UTF8ToWide(kSwitch2).c_str()));

  // And then test that the function to delete the RunOnce entry also works.
  post_reboot.UnregisterRunOnceOnRestart();
  EXPECT_FALSE(RunOnceCommandLineContains(
      TEST_PRODUCT_SHORTNAME_STRING, base::UTF8ToWide(cleanup_id).c_str()));

  // Attempt to unregister RunOnce again to make sure that nothing weird happens
  // if the key doesn't exist.
  post_reboot.UnregisterRunOnceOnRestart();
  EXPECT_FALSE(RunOnceCommandLineContains(
      TEST_PRODUCT_SHORTNAME_STRING, base::UTF8ToWide(cleanup_id).c_str()));
}

TEST(PostRebootRegistrationTests, ReadRunOncePostRebootCommandLine) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);

  PostRebootRegistration post_reboot(TEST_PRODUCT_SHORTNAME_STRING);

  std::string cleanup_id("my_unique_test_cleanup_id");

  base::CommandLine tmp_cmd(base::CommandLine::NO_PROGRAM);
  EXPECT_FALSE(
      post_reboot.ReadRunOncePostRebootCommandLine(cleanup_id, &tmp_cmd));

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(kSwitch1);
  command_line.AppendSwitch(kSwitch2);

  // Populate the registry with some command line.
  EXPECT_TRUE(post_reboot.RegisterRunOnceOnRestart(cleanup_id, command_line));

  EXPECT_TRUE(
      post_reboot.ReadRunOncePostRebootCommandLine(cleanup_id, &tmp_cmd));
  EXPECT_TRUE(tmp_cmd.HasSwitch(kSwitch1));
  EXPECT_TRUE(tmp_cmd.HasSwitch(kSwitch2));

  // Verify that ReadRunOncePostRebootCommandLine properly deleted the registry
  // key.
  EXPECT_FALSE(
      post_reboot.ReadRunOncePostRebootCommandLine(cleanup_id, &tmp_cmd));
  EXPECT_FALSE(RunOnceOverrideCommandLineContains(
      cleanup_id, base::UTF8ToWide(kSwitch1).c_str()));

  // Check that command lines that are too long will not be registered.
  constexpr int max_command_line_length = 260;
  // Ensure that the modified and shortened command line will be too long by
  // passing in a very long cleanup ID.
  std::string long_cleanup_id(max_command_line_length, L'a');
  EXPECT_FALSE(
      post_reboot.RegisterRunOnceOnRestart(long_cleanup_id, command_line));
  EXPECT_FALSE(
      post_reboot.ReadRunOncePostRebootCommandLine(long_cleanup_id, &tmp_cmd));
}

}  // namespace chrome_cleaner
