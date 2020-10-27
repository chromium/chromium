// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths_internal.h"

#include "base/files/file_path.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {
namespace {

const char kLsbRelease[] =
    "CHROMEOS_RELEASE_NAME=Chrome OS\n"
    "CHROMEOS_RELEASE_VERSION=1.2.3.4\n";

// Overrides base::SysInfo::IsRunningOnChromeOS() to return true.
class ScopedIsRunningOnChromeOS {
 public:
  ScopedIsRunningOnChromeOS() {
    base::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, base::Time());
  }
  ~ScopedIsRunningOnChromeOS() {
    base::SysInfo::SetChromeOSVersionInfoForTest("", base::Time());
  }
};

TEST(ChromePaths, UserDataDirectoryIsInsideEncryptedPartition) {
  // Force paths to behave like they do on device.
  ScopedIsRunningOnChromeOS is_running_on_chromeos;
  base::FilePath user_data_dir;
  ASSERT_TRUE(GetDefaultUserDataDirectory(&user_data_dir));
  // The Lacros user data directory contains profile information, including
  // credentials. It must be inside the encrypted system user partition.
  base::FilePath home_chronos_user(crosapi::kHomeChronosUserPath);
  EXPECT_TRUE(home_chronos_user.IsParent(user_data_dir));
}

}  // namespace
}  // namespace chrome
