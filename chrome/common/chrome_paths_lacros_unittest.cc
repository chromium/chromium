// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths_internal.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/test/scoped_running_on_chromeos.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {
namespace {

TEST(ChromePaths, UserDataDirectoryIsInsideEncryptedPartition) {
  // Force paths to behave like they do on device.
  base::test::ScopedRunningOnChromeOS running_on_chromeos;
  base::FilePath user_data_dir;
  ASSERT_TRUE(GetDefaultUserDataDirectory(&user_data_dir));
  // The Lacros user data directory contains profile information, including
  // credentials. It must be inside the encrypted system user partition.
  base::FilePath home_chronos_user("/home/chronos/user");
  EXPECT_TRUE(home_chronos_user.IsParent(user_data_dir));
}

TEST(ChromePaths, DownloadsDirectoryRestoredAfterScopedPathOverride) {
  base::FilePath original_path;
  base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &original_path);
  {
    // Override with a temp directory.
    base::ScopedPathOverride override(chrome::DIR_DEFAULT_DOWNLOADS);
    base::FilePath new_path;
    base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &new_path);
    EXPECT_NE(original_path, new_path);
  }
  // Original path is restored.
  base::FilePath restored_path;
  base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &restored_path);
  EXPECT_EQ(original_path, restored_path);
}

}  // namespace
}  // namespace chrome
