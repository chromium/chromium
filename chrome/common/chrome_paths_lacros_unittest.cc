// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths_internal.h"

#include "base/files/file_path.h"
#include "base/test/scoped_running_on_chromeos.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
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
  base::FilePath home_chronos_user(crosapi::kHomeChronosUserPath);
  EXPECT_TRUE(home_chronos_user.IsParent(user_data_dir));
}

}  // namespace
}  // namespace chrome
