// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/proximity_auth/proximity_auth_profile_pref_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "chromeos/ash/components/proximity_auth/proximity_auth_pref_names.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace proximity_auth {
namespace {

const int64_t kPromotionCheckTimestampMs1 = 1111111111L;
const int64_t kPromotionCheckTimestampMs2 = 2222222222L;

}  //  namespace

class ProximityAuthProfilePrefManagerTest : public testing::Test {
 public:
  ProximityAuthProfilePrefManagerTest(
      const ProximityAuthProfilePrefManagerTest&) = delete;
  ProximityAuthProfilePrefManagerTest& operator=(
      const ProximityAuthProfilePrefManagerTest&) = delete;

 protected:
  ProximityAuthProfilePrefManagerTest() = default;

  void SetUp() override {
    fake_multidevice_setup_client_ =
        std::make_unique<ash::multidevice_setup::FakeMultiDeviceSetupClient>();
    ProximityAuthProfilePrefManager::RegisterPrefs(pref_service_.registry());
    ash::multidevice_setup::RegisterFeaturePrefs(pref_service_.registry());
    pref_manager_ = std::make_unique<ProximityAuthProfilePrefManager>(
        &pref_service_, fake_multidevice_setup_client_.get());
  }

  std::unique_ptr<ash::multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<ProximityAuthProfilePrefManager> pref_manager_;
};

// TODO(crbug/904005): Investigate using Unified Setup API instead of directly
// manipulating the pref.
TEST_F(ProximityAuthProfilePrefManagerTest, IsEasyUnlockAllowed) {
  EXPECT_TRUE(pref_manager_->IsEasyUnlockAllowed());

  // Simulating setting kEasyUnlockAllowed pref through enterprise policy.
  pref_service_.SetBoolean(ash::multidevice_setup::kSmartLockAllowedPrefName,
                           false);
  EXPECT_FALSE(pref_manager_->IsEasyUnlockAllowed());
}

TEST_F(ProximityAuthProfilePrefManagerTest, LastPromotionCheckTimestamp) {
  EXPECT_EQ(0L, pref_manager_->GetLastPromotionCheckTimestampMs());
  pref_manager_->SetLastPromotionCheckTimestampMs(kPromotionCheckTimestampMs1);
  EXPECT_EQ(kPromotionCheckTimestampMs1,
            pref_manager_->GetLastPromotionCheckTimestampMs());
  pref_manager_->SetLastPromotionCheckTimestampMs(kPromotionCheckTimestampMs2);
  EXPECT_EQ(kPromotionCheckTimestampMs2,
            pref_manager_->GetLastPromotionCheckTimestampMs());
}

TEST_F(ProximityAuthProfilePrefManagerTest, PromotionShownCount) {
  EXPECT_EQ(0, pref_manager_->GetPromotionShownCount());
  pref_manager_->SetPromotionShownCount(1);
  EXPECT_EQ(1, pref_manager_->GetPromotionShownCount());
  pref_manager_->SetPromotionShownCount(2);
  EXPECT_EQ(2, pref_manager_->GetPromotionShownCount());
}

}  // namespace proximity_auth
