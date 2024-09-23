// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/vapid_key_manager.h"

#include "components/sharing_message/features.h"
#include "components/sharing_message/sharing_sync_preference.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "crypto/ec_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class VapidKeyManagerTest : public testing::Test {
 protected:
  VapidKeyManagerTest()
      : sharing_sync_preference_(&prefs_, &fake_device_info_sync_service_),
        vapid_key_manager_(&sharing_sync_preference_, &test_sync_service_) {
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
  }

  sync_preferences::TestingPrefServiceSyncable prefs_;
  syncer::FakeDeviceInfoSyncService fake_device_info_sync_service_;
  SharingSyncPreference sharing_sync_preference_;
  syncer::TestSyncService test_sync_service_;
  VapidKeyManager vapid_key_manager_;
};

}  // namespace

TEST_F(VapidKeyManagerTest, CreateKeyFlow) {
  // No keys stored in preferences.
  EXPECT_EQ(std::nullopt, sharing_sync_preference_.GetVapidKey());

  // Expected to create new keys and store in preferences.
  crypto::ECPrivateKey* key_1 = vapid_key_manager_.GetOrCreateKey();
  EXPECT_TRUE(key_1);
  std::vector<uint8_t> key_info;
  EXPECT_TRUE(key_1->ExportPrivateKey(&key_info));
  EXPECT_EQ(key_info, sharing_sync_preference_.GetVapidKey());

  // Expected to return same key when called again.
  crypto::ECPrivateKey* key_2 = vapid_key_manager_.GetOrCreateKey();
  EXPECT_TRUE(key_2);
  std::vector<uint8_t> key_info_2;
  EXPECT_TRUE(key_2->ExportPrivateKey(&key_info_2));
  EXPECT_EQ(key_info, key_info_2);
}

TEST_F(VapidKeyManagerTest, SkipCreateKeyFlow) {
  test_sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());

  // No keys stored in preferences.
  EXPECT_EQ(std::nullopt, sharing_sync_preference_.GetVapidKey());

  // Expected to skip creating keys when sync preference is not available.
  EXPECT_FALSE(vapid_key_manager_.GetOrCreateKey());

  // Refreshing keys still won't generate keys.
  vapid_key_manager_.RefreshCachedKey();
  EXPECT_FALSE(vapid_key_manager_.GetOrCreateKey());
}

TEST_F(VapidKeyManagerTest, ReadFromPreferenceFlow) {
  // VAPID key already stored in preferences.
  auto preference_key_1 = crypto::ECPrivateKey::Create();
  ASSERT_TRUE(preference_key_1);
  std::vector<uint8_t> preference_key_info_1;
  ASSERT_TRUE(preference_key_1->ExportPrivateKey(&preference_key_info_1));
  sharing_sync_preference_.SetVapidKey(preference_key_info_1);

  // Expected to return key stored in preferences.
  crypto::ECPrivateKey* key_1 = vapid_key_manager_.GetOrCreateKey();
  EXPECT_TRUE(key_1);
  std::vector<uint8_t> key_info_1;
  EXPECT_TRUE(key_1->ExportPrivateKey(&key_info_1));
  EXPECT_EQ(preference_key_info_1, key_info_1);

  // Change VAPID key in sync prefernece.
  auto preference_key_2 = crypto::ECPrivateKey::Create();
  ASSERT_TRUE(preference_key_2);
  std::vector<uint8_t> preference_key_info_2;
  ASSERT_TRUE(preference_key_2->ExportPrivateKey(&preference_key_info_2));
  sharing_sync_preference_.SetVapidKey(preference_key_info_2);

  // Refresh local cache with new key in sync preference.
  EXPECT_TRUE(vapid_key_manager_.RefreshCachedKey());
  crypto::ECPrivateKey* key_2 = vapid_key_manager_.GetOrCreateKey();
  EXPECT_TRUE(key_2);
  std::vector<uint8_t> key_info_2;
  EXPECT_TRUE(key_2->ExportPrivateKey(&key_info_2));
  EXPECT_EQ(preference_key_info_2, key_info_2);
}
