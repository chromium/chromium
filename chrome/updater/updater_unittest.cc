// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/updater/mac/mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
#include "base/file_version_info_win.h"
#endif

namespace updater {

// Tests the updater process returns 0 when run with --test argument.
TEST(UpdaterTest, UpdaterExitCode) {
  base::FilePath this_executable_path;
  ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &this_executable_path));
  const base::FilePath executableFolder = this_executable_path.DirName();
  const base::FilePath updater = this_executable_path.DirName().Append(
      updater::GetExecutableRelativePath());

  base::LaunchOptions options;
#if BUILDFLAG(IS_WIN)
  options.start_hidden = true;
#endif  // BUILDFLAG(IS_WIN)

  base::CommandLine command_line(updater);
  command_line.AppendSwitch("test");
  auto process = base::LaunchProcess(command_line, options);
  ASSERT_TRUE(process.IsValid());
  int exit_code = -1;
  EXPECT_TRUE(process.WaitForExitWithTimeout(base::Seconds(60), &exit_code));
  EXPECT_EQ(0, exit_code);
}

#if BUILDFLAG(IS_WIN)
// Tests that the updater test target version resource contains specific
// information to disambiguate the binary. For Windows builds and during tests,
// the "updater_test.exe" file is being installed as "updater.exe", therefore
// it is useful to tell them apart.
TEST(UpdaterTest, UpdaterTestVersionResource) {
  base::FilePath this_executable_path;
  ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &this_executable_path));

  const base::FilePath executable_test(FILE_PATH_LITERAL("updater_test.exe"));
  const std::unique_ptr<FileVersionInfoWin> version_info =
      FileVersionInfoWin::CreateFileVersionInfoWin(
          this_executable_path.DirName().Append(executable_test));

  EXPECT_EQ(version_info->original_filename(), executable_test.AsUTF16Unsafe());
}
#endif

}  // namespace updater
