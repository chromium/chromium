// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/device_id_helper.h"

#include <string>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {
namespace {

#if !BUILDFLAG(IS_CHROMEOS_ASH)

TEST(DeviceIdHelper, GenerateSigninScopedDeviceId) {
  EXPECT_FALSE(GenerateSigninScopedDeviceId().empty());
  EXPECT_NE(GenerateSigninScopedDeviceId(), GenerateSigninScopedDeviceId());
}

TEST(DeviceIdHelper, RecreateSigninScopedDeviceId) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  prefs.registry()->RegisterStringPref(
      prefs::kGoogleServicesSigninScopedDeviceId, std::string());
  ASSERT_TRUE(
      prefs.GetString(prefs::kGoogleServicesSigninScopedDeviceId).empty());

  std::string device_id_1 = RecreateSigninScopedDeviceId(&prefs);
  EXPECT_FALSE(device_id_1.empty());
  EXPECT_EQ(device_id_1,
            prefs.GetString(prefs::kGoogleServicesSigninScopedDeviceId));

  std::string device_id_2 = RecreateSigninScopedDeviceId(&prefs);
  EXPECT_FALSE(device_id_2.empty());
  EXPECT_NE(device_id_1, device_id_2);
  EXPECT_EQ(device_id_2,
            prefs.GetString(prefs::kGoogleServicesSigninScopedDeviceId));
}

TEST(DeviceIdHelper, GetSigninScopedDeviceId) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  prefs.registry()->RegisterStringPref(
      prefs::kGoogleServicesSigninScopedDeviceId, std::string());

  ASSERT_TRUE(
      prefs.GetString(prefs::kGoogleServicesSigninScopedDeviceId).empty());

  std::string device_id_1 = GetSigninScopedDeviceId(&prefs);
  EXPECT_FALSE(device_id_1.empty());
  EXPECT_EQ(device_id_1,
            prefs.GetString(prefs::kGoogleServicesSigninScopedDeviceId));

  std::string device_id_2 = GetSigninScopedDeviceId(&prefs);
  EXPECT_EQ(device_id_1, device_id_2);
  EXPECT_EQ(device_id_2,
            prefs.GetString(prefs::kGoogleServicesSigninScopedDeviceId));
}

#endif

}  // namespace
}  // namespace signin
