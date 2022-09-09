// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/installer/installer.h"

#include <shlobj.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/win/installer/exit_code.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests that `HandleRunElevated` returns `UNABLE_TO_ELEVATE_METAINSTALLER` when
// not elevated and called with `kCmdLineExpectElevated` argument.
TEST(InstallerTest, HandleRunElevated) {
  if (::IsUserAnAdmin())
    return;

  base::CommandLine command_line(
      base::FilePath(FILE_PATH_LITERAL("UpdaterSetup.exe")));
  command_line.AppendSwitch(updater::kInstallSwitch);
  command_line.AppendSwitch(updater::kSystemSwitch);
  command_line.AppendSwitch(updater::kCmdLineExpectElevated);

  updater::ProcessExitResult exit_result =
      updater::HandleRunElevated(command_line);
  EXPECT_EQ(exit_result.exit_code, updater::UNABLE_TO_ELEVATE_METAINSTALLER);
  EXPECT_EQ(exit_result.windows_error, 0U);
}
