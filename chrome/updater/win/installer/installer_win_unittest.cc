// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/installer/installer.h"

#include <shlobj.h>

#include <optional>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/win/installer/exit_code.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests that `HandleRunElevated` returns `UNABLE_TO_ELEVATE_METAINSTALLER` when
// not elevated and called with `kCmdLineExpectElevated` argument.
TEST(InstallerTest, HandleRunElevated) {
  if (::IsUserAnAdmin()) {
    return;
  }

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

TEST(InstallerTest, FindOfflineDir) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath unpack_path = temp_dir.GetPath();
  base::FilePath metainstall_dir = unpack_path.Append(L"bin");
  ASSERT_TRUE(base::CreateDirectory(metainstall_dir));

  EXPECT_FALSE(updater::FindOfflineDir(unpack_path).has_value());

  base::FilePath offline_install_dir =
      metainstall_dir.Append(L"Offline")
          .Append(L"{8D5D0563-F2A0-40E3-932D-AFEAE261A9D1}");
  ASSERT_TRUE(base::CreateDirectory(offline_install_dir));

  std::optional<base::FilePath> offline_dir =
      updater::FindOfflineDir(unpack_path);
  EXPECT_TRUE(offline_dir.has_value());
  EXPECT_EQ(offline_dir->BaseName(),
            base::FilePath(L"{8D5D0563-F2A0-40E3-932D-AFEAE261A9D1}"));
}
