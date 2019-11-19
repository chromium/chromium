// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths_win.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/test/test_timeouts.h"
#include "chrome/chrome_cleaner/pup_data/test_uws.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(GenerateTestUwsTest, WriteTestUwS) {
  // Ensure the expected output files don't exist.
  base::FilePath start_menu_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_START_MENU, &start_menu_path));
  base::FilePath startup_dir =
      start_menu_path.Append(STRING16_LITERAL("Startup"));

  base::FilePath uws_file_a =
      startup_dir.Append(chrome_cleaner::kTestUwsAFilename);
  ASSERT_TRUE(base::DeleteFile(uws_file_a, /*recursive=*/false));

  base::FilePath uws_file_b =
      startup_dir.Append(chrome_cleaner::kTestUwsBFilename);
  ASSERT_TRUE(base::DeleteFile(uws_file_b, /*recursive=*/false));

  // Delete the output files on exit, including on early exit.
  base::ScopedClosureRunner delete_uws_file_a(base::BindOnce(
      base::IgnoreResult(&base::DeleteFile), uws_file_a, /*recursive=*/false));
  base::ScopedClosureRunner delete_uws_file_b(base::BindOnce(
      base::IgnoreResult(&base::DeleteFile), uws_file_b, /*recursive=*/false));

  // Expect generate_test_uws to finish quickly with exit code 0 (success).
  base::Process process(base::LaunchProcess(
      STRING16_LITERAL("generate_test_uws.exe"), base::LaunchOptions()));
  ASSERT_TRUE(process.IsValid());

  int exit_code = -1;
  bool exited_within_timeout = process.WaitForExitWithTimeout(
      TestTimeouts::action_timeout(), &exit_code);
  EXPECT_TRUE(exited_within_timeout);
  EXPECT_EQ(exit_code, 0);

  if (!exited_within_timeout)
    process.Terminate(/*exit_code=*/-1, /*wait=*/false);

  // Verify that generate_test_uws created the expected files.
  std::string uws_file_contents_a;
  EXPECT_TRUE(base::ReadFileToStringWithMaxSize(
      uws_file_a, &uws_file_contents_a,
      chrome_cleaner::kTestUwsAFileContentsSize))
      << uws_file_a;
  EXPECT_EQ(uws_file_contents_a, chrome_cleaner::kTestUwsAFileContents);

  std::string uws_file_contents_b;
  EXPECT_TRUE(base::ReadFileToStringWithMaxSize(
      uws_file_b, &uws_file_contents_b,
      chrome_cleaner::kTestUwsBFileContentsSize))
      << uws_file_b;
  EXPECT_EQ(uws_file_contents_b, chrome_cleaner::kTestUwsBFileContents);
}

}  // namespace
