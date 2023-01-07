// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/local_device_info_util.h"

#include <utility>

#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/command_line.h"
#include "base/test/scoped_chromeos_version_info.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace syncer {
namespace {

// Call GetPersonalizableDeviceNameBlocking and make sure its return
// value looks sane.
TEST(GetClientNameTest, GetPersonalizableDeviceNameBlocking) {
  const std::string& client_name = GetPersonalizableDeviceNameBlocking();
  EXPECT_FALSE(client_name.empty());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Call GetPersonalizableDeviceNameBlocking on ChromeOS where the
// board type is CHROMEBOOK and make sure the return value is "Chromebook".
TEST(GetClientNameTest, GetPersonalizableDeviceNameBlockingChromebook) {
  const char* kLsbRelease = "DEVICETYPE=CHROMEBOOK\n";
  base::test::ScopedChromeOSVersionInfo version(kLsbRelease, base::Time());
  const std::string& client_name = GetPersonalizableDeviceNameBlocking();
  EXPECT_EQ("Chromebook", client_name);
}

// Call GetPersonalizableDeviceNameBlocking on ChromeOS where the
// board type is a CHROMEBOX and make sure the return value is "Chromebox".
TEST(GetClientNameTest, GetPersonalizableDeviceNameBlockingChromebox) {
  const char* kLsbRelease = "DEVICETYPE=CHROMEBOX\n";
  base::test::ScopedChromeOSVersionInfo version(kLsbRelease, base::Time());
  const std::string& client_name = GetPersonalizableDeviceNameBlocking();
  EXPECT_EQ("Chromebox", client_name);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
}  // namespace syncer
