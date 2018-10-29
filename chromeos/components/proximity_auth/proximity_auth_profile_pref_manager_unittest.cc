// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/proximity_auth_profile_pref_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/components/proximity_auth/proximity_auth_local_state_pref_manager.h"
#include "chromeos/components/proximity_auth/proximity_auth_pref_names.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace proximity_auth {
namespace {

const char kUserEmail[] = "testuser@example.com";

const int64_t kPromotionCheckTimestampMs1 = 1111111111L;
const int64_t kPromotionCheckTimestampMs2 = 2222222222L;

const ProximityAuthPrefManager::ProximityThreshold kProximityThreshold1 =
    ProximityAuthPrefManager::ProximityThreshold::kFar;
const ProximityAuthPrefManager::ProximityThreshold kProximityThreshold2 =
    ProximityAuthPrefManager::ProximityThreshold::kVeryFar;

}  //  namespace

class ProximityAuthProfilePrefManagerTest : public testing::Test {
 protected:
  ProximityAuthProfilePrefManagerTest() {}

  void SetUp() override {
    ProximityAuthProfilePrefManager::RegisterPrefs(pref_service_.registry());
    chromeos::multidevice_setup::RegisterFeaturePrefs(pref_service_.registry());

    scoped_feature_list_.InitWithFeatures(
        std::vector<base::Feature>() /* enable_features */,
        std::vector<base::Feature>{
            chromeos::features::kMultiDeviceApi,
            chromeos::features::
                kEnableUnifiedMultiDeviceSetup} /* disable_features */);
  }

  sync_preferences::TestingPrefServiceSyncable pref_service_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(ProximityAuthProfilePrefManagerTest);
};

TEST_F(ProximityAuthProfilePrefManagerTest, IsEasyUnlockAllowed) {
  ProximityAuthProfilePrefManager pref_manager(
      &pref_service_, nullptr /* multidevice_setup_service */);
  EXPECT_TRUE(pref_manager.IsEasyUnlockAllowed());

  // Simulating setting kEasyUnlockAllowed pref through enterprise policy.
  pref_service_.SetBoolean(
      chromeos::multidevice_setup::kSmartLockAllowedPrefName, false);
  EXPECT_FALSE(pref_manager.IsEasyUnlockAllowed());
}

TEST_F(ProximityAuthProfilePrefManagerTest, IsEasyUnlockEnabled) {
  ProximityAuthProfilePrefManager pref_manager(
      &pref_service_, nullptr /* multidevice_setup_service */);
  EXPECT_TRUE(pref_manager.IsEasyUnlockEnabled());

  pref_manager.SetIsEasyUnlockEnabled(true);
  EXPECT_TRUE(pref_manager.IsEasyUnlockEnabled());

  pref_manager.SetIsEasyUnlockEnabled(false);
  EXPECT_FALSE(pref_manager.IsEasyUnlockEnabled());
}

TEST_F(ProximityAuthProfilePrefManagerTest, LastPromotionCheckTimestamp) {
  ProximityAuthProfilePrefManager pref_manager(
      &pref_service_, nullptr /* multidevice_setup_service */);
  EXPECT_EQ(0L, pref_manager.GetLastPromotionCheckTimestampMs());
  pref_manager.SetLastPromotionCheckTimestampMs(kPromotionCheckTimestampMs1);
  EXPECT_EQ(kPromotionCheckTimestampMs1,
            pref_manager.GetLastPromotionCheckTimestampMs());
  pref_manager.SetLastPromotionCheckTimestampMs(kPromotionCheckTimestampMs2);
  EXPECT_EQ(kPromotionCheckTimestampMs2,
            pref_manager.GetLastPromotionCheckTimestampMs());
}

TEST_F(ProximityAuthProfilePrefManagerTest, PromotionShownCount) {
  ProximityAuthProfilePrefManager pref_manager(
      &pref_service_, nullptr /* multidevice_setup_service */);
  EXPECT_EQ(0, pref_manager.GetPromotionShownCount());
  pref_manager.SetPromotionShownCount(1);
  EXPECT_EQ(1, pref_manager.GetPromotionShownCount());
  pref_manager.SetPromotionShownCount(2);
  EXPECT_EQ(2, pref_manager.GetPromotionShownCount());
}

TEST_F(ProximityAuthProfilePrefManagerTest, ProximityThreshold) {
  ProximityAuthProfilePrefManager pref_manager(
      &pref_service_, nullptr /* multidevice_setup_service */);
  EXPECT_EQ(1, pref_manager.GetProximityThreshold());
  pref_manager.SetProximityThreshold(kProximityThreshold1);
  EXPECT_EQ(kProximityThreshold1, pref_manager.GetProximityThreshold());
  pref_manager.SetProximityThreshold(kProximityThreshold2);
  EXPECT_EQ(kProximityThreshold2, pref_manager.GetProximityThreshold());
}

TEST_F(ProximityAuthProfilePrefManagerTest, IsChromeOSLoginEnabled) {
  ProximityAuthProfilePrefManager pref_manager(
      &pref_service_, nullptr /* multidevice_setup_service */);
  EXPECT_FALSE(pref_manager.IsChromeOSLoginEnabled());

  pref_manager.SetIsChromeOSLoginEnabled(true);
  EXPECT_TRUE(pref_manager.IsChromeOSLoginEnabled());

  pref_manager.SetIsChromeOSLoginEnabled(false);
  EXPECT_FALSE(pref_manager.IsChromeOSLoginEnabled());
}

TEST_F(ProximityAuthProfilePrefManagerTest, SyncsToLocalPrefOnChange) {
  ProximityAuthProfilePrefManager profile_pref_manager(
      &pref_service_, nullptr /* multidevice_setup_service */);

  TestingPrefServiceSimple local_state;
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  ProximityAuthLocalStatePrefManager::RegisterPrefs(local_state.registry());
  profile_pref_manager.StartSyncingToLocalState(&local_state, account_id);

  // Use the local state pref manager to verify that prefs are synced correctly.
  ProximityAuthLocalStatePrefManager local_pref_manager(&local_state);
  local_pref_manager.SetActiveUser(account_id);

  profile_pref_manager.SetIsChromeOSLoginEnabled(true);
  profile_pref_manager.SetIsEasyUnlockEnabled(true);
  profile_pref_manager.SetProximityThreshold(kProximityThreshold1);
  EXPECT_TRUE(local_pref_manager.IsChromeOSLoginEnabled());
  EXPECT_TRUE(local_pref_manager.IsEasyUnlockEnabled());
  EXPECT_EQ(kProximityThreshold1, local_pref_manager.GetProximityThreshold());

  profile_pref_manager.SetIsChromeOSLoginEnabled(false);
  profile_pref_manager.SetIsEasyUnlockEnabled(false);
  profile_pref_manager.SetProximityThreshold(kProximityThreshold2);
  EXPECT_FALSE(local_pref_manager.IsChromeOSLoginEnabled());
  EXPECT_FALSE(local_pref_manager.IsEasyUnlockEnabled());
  EXPECT_EQ(kProximityThreshold2, local_pref_manager.GetProximityThreshold());

  // Test changing the kEasyUnlockAllowed pref value directly (e.g. through
  // enterprise policy).
  EXPECT_TRUE(local_pref_manager.IsEasyUnlockAllowed());
  pref_service_.SetBoolean(
      chromeos::multidevice_setup::kSmartLockAllowedPrefName, false);
  EXPECT_FALSE(profile_pref_manager.IsEasyUnlockAllowed());
  EXPECT_FALSE(local_pref_manager.IsEasyUnlockAllowed());
}

}  // namespace proximity_auth
