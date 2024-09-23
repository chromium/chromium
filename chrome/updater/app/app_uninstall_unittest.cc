// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_uninstall.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/test/unit_test_util.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

class AppUninstallTest : public testing::Test {
 private:
  base::test::TaskEnvironment environment_;
};

TEST_F(AppUninstallTest, GetVersionExecutablePaths) {
#if BUILDFLAG(IS_MAC)
  // Cannot create global prefs in the macOS system scope.
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    return;
  }
#endif  // BUILDFLAG(IS_MAC)

  for (int major_version_offset : {-2, -1, 0, 1}) {
    updater::test::SetupFakeUpdaterVersion(
        GetUpdaterScopeForTesting(), base::Version(kUpdaterVersion),
        major_version_offset,
        /*should_create_updater_executable=*/true);
  }

  for (int major_version_offset : {2, 3}) {
    updater::test::SetupFakeUpdaterVersion(
        GetUpdaterScopeForTesting(), base::Version(kUpdaterVersion),
        major_version_offset,
        /*should_create_updater_executable=*/false);
  }

  ASSERT_EQ(GetVersionExecutablePaths(GetUpdaterScopeForTesting()).size(), 5u);

  const std::optional<base::FilePath> path =
      GetInstallDirectory(GetUpdaterScopeForTesting());
  ASSERT_TRUE(path);
  ASSERT_TRUE(base::DeletePathRecursively(*path));
}

TEST_F(AppUninstallTest, GetUninstallSelfCommandLine) {
  const base::CommandLine command_line(GetUninstallSelfCommandLine(
      GetUpdaterScopeForTesting(),
      base::FilePath(FILE_PATH_LITERAL("foobar.executable"))));

  EXPECT_TRUE(command_line.HasSwitch(kUninstallSelfSwitch));
  EXPECT_EQ(command_line.HasSwitch(kSystemSwitch),
            IsSystemInstall(GetUpdaterScopeForTesting()));
}

}  // namespace updater
