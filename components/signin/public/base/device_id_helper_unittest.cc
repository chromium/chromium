// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/device_id_helper.h"

#include <string>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {
namespace {

#if !BUILDFLAG(IS_CHROMEOS)

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

#else

constexpr char kEphemeralPrefix[] = "t_";

TEST(DeviceIdHelper, NonEphemeralDeviceIdsAreNotEmpty) {
  EXPECT_FALSE(GenerateSigninScopedDeviceId(/*for_ephemeral=*/false).empty());
}

TEST(DeviceIdHelper, NonEphemeralDeviceIdsDoNotHaveTheEphemeralPrefix) {
  EXPECT_FALSE(GenerateSigninScopedDeviceId(/*for_ephemeral=*/false)
                   .starts_with(kEphemeralPrefix));
}

TEST(DeviceIdHelper,
     NonEphemeralDeviceIdsAreUniqueIfStableDeviceIdFeatureIsDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(switches::kStableDeviceId);

  const std::string device_id1 =
      GenerateSigninScopedDeviceId(/*for_ephemeral=*/false);

  const std::string device_id2 =
      GenerateSigninScopedDeviceId(/*for_ephemeral=*/false);

  // Newly generated id is not the same as the previous one.
  EXPECT_NE(device_id2, device_id1);
}

TEST(DeviceIdHelper,
     NonEphemeralDeviceIdsAreNotUniqueIfStableDeviceIdFeatureIsEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(switches::kStableDeviceId);

  const std::string device_id1 =
      GenerateSigninScopedDeviceId(/*for_ephemeral=*/false);

  const std::string device_id2 =
      GenerateSigninScopedDeviceId(/*for_ephemeral=*/false);

  // Newly generated id is the same as the previous one.
  EXPECT_EQ(device_id2, device_id1);
}

TEST(DeviceIdHelper, EphemeralDeviceIdsAreNotEmpty) {
  EXPECT_FALSE(GenerateSigninScopedDeviceId(/*for_ephemeral=*/true).empty());
}

TEST(DeviceIdHelper, EphemeralDeviceIdsHaveTheEphemeralPrefix) {
  EXPECT_TRUE(GenerateSigninScopedDeviceId(/*for_ephemeral=*/true)
                  .starts_with(kEphemeralPrefix));
}

TEST(DeviceIdHelper,
     EphemeralDeviceIdsAreUniqueIfStableDeviceIdFeatureIsDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(switches::kStableDeviceId);

  const std::string device_id1 =
      GenerateSigninScopedDeviceId(/*for_ephemeral=*/true);

  const std::string device_id2 =
      GenerateSigninScopedDeviceId(/*for_ephemeral=*/true);

  // Newly generated id is not the same as the previous one.
  EXPECT_NE(device_id2, device_id1);
}

TEST(DeviceIdHelper,
     EphemeralDeviceIdsAreUniqueIfStableDeviceIdFeatureIsEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(switches::kStableDeviceId);

  const std::string device_id1 =
      GenerateSigninScopedDeviceId(/*for_ephemeral=*/true);

  const std::string device_id2 =
      GenerateSigninScopedDeviceId(/*for_ephemeral=*/true);

  // Newly generated id is not the same as the previous one.
  EXPECT_NE(device_id2, device_id1);
}

#endif

}  // namespace
}  // namespace signin
