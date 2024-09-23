// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_sync_preference.h"

#include <memory>

#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sharing_message/fake_device_info.h"
#include "components/sharing_message/pref_names.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_device_info/fake_local_device_info_provider.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kVapidKeyStr[] = "test_vapid_key";
const std::vector<uint8_t> kVapidKey =
    std::vector<uint8_t>(std::begin(kVapidKeyStr), std::end(kVapidKeyStr));

const char kDeviceVapidFcmToken[] = "test_vapid_fcm_token";
const char kDeviceVapidAuthToken[] = "test_vapid_auth_token";
const char kDeviceVapidP256dh[] = "test_vapid_p256dh";
const char kDeviceSenderIdFcmToken[] = "test_sender_id_fcm_token";
const char kDeviceSenderIdAuthToken[] = "test_sender_id_auth_token";
const char kDeviceSenderIdP256dh[] = "test_sender_id_p256dh";

const char kAuthorizedEntity[] = "authorized_entity";

const char kSharingInfoEnabledFeatures[] = "enabled_features";

}  // namespace

class SharingSyncPreferenceTest : public testing::Test {
 protected:
  SharingSyncPreferenceTest()
      : sharing_sync_preference_(&prefs_, &fake_device_info_sync_service_) {
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
  }

  syncer::DeviceInfo::SharingInfo GetDefaultSharingInfo() {
    return syncer::DeviceInfo::SharingInfo(
        {kDeviceVapidFcmToken, kDeviceVapidP256dh, kDeviceVapidAuthToken},
        {kDeviceSenderIdFcmToken, kDeviceSenderIdP256dh,
         kDeviceSenderIdAuthToken},
        /*chime_representative_target_id=*/std::string(),
        std::set<sync_pb::SharingSpecificFields::EnabledFeatures>{
            sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2});
  }

  void AddEnabledFeature(int feature) {
    const base::Value::Dict& registration =
        prefs_.GetDict(prefs::kSharingLocalSharingInfo);
    base::Value::List enabled_features =
        registration.FindList(kSharingInfoEnabledFeatures)->Clone();

    enabled_features.Append(feature);

    ScopedDictPrefUpdate local_sharing_info_update(
        &prefs_, prefs::kSharingLocalSharingInfo);
    local_sharing_info_update->Set(kSharingInfoEnabledFeatures,
                                   std::move(enabled_features));
  }

  sync_preferences::TestingPrefServiceSyncable prefs_;
  syncer::FakeDeviceInfoSyncService fake_device_info_sync_service_;
  SharingSyncPreference sharing_sync_preference_;
};

TEST_F(SharingSyncPreferenceTest, UpdateVapidKeys) {
  EXPECT_EQ(std::nullopt, sharing_sync_preference_.GetVapidKey());
  sharing_sync_preference_.SetVapidKey(kVapidKey);
  EXPECT_EQ(kVapidKey, sharing_sync_preference_.GetVapidKey());
}

TEST_F(SharingSyncPreferenceTest, SyncAndRemoveLocalDevice) {
  const syncer::DeviceInfo* local_device_info =
      fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
          ->GetLocalDeviceInfo();
  fake_device_info_sync_service_.GetDeviceInfoTracker()->Add(local_device_info);

  // Setting SharingInfo should trigger RefreshLocalDeviceInfoCount.
  auto sharing_info = GetDefaultSharingInfo();
  sharing_sync_preference_.SetLocalSharingInfo(sharing_info);
  EXPECT_EQ(1, fake_device_info_sync_service_.RefreshLocalDeviceInfoCount());

  // Assume LocalDeviceInfoProvider is updated now.
  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
      ->GetMutableDeviceInfo()
      ->set_sharing_info(sharing_info);

  // Setting exactly the same SharingInfo in LocalDeviceInfoProvider shouldn't
  // trigger RefreshLocalDeviceInfoCount.
  sharing_sync_preference_.SetLocalSharingInfo(sharing_info);
  EXPECT_EQ(1, fake_device_info_sync_service_.RefreshLocalDeviceInfoCount());

  // Clearing SharingInfo should trigger RefreshLocalDeviceInfoCount.
  sharing_sync_preference_.ClearLocalSharingInfo();
  EXPECT_EQ(2, fake_device_info_sync_service_.RefreshLocalDeviceInfoCount());
}

TEST_F(SharingSyncPreferenceTest, FCMRegistrationGetSet) {
  EXPECT_FALSE(sharing_sync_preference_.GetFCMRegistration());

  base::Time time_now = base::Time::Now();
  sharing_sync_preference_.SetFCMRegistration(
      SharingSyncPreference::FCMRegistration(kAuthorizedEntity, time_now));

  auto fcm_registration = sharing_sync_preference_.GetFCMRegistration();
  EXPECT_TRUE(fcm_registration);
  EXPECT_EQ(kAuthorizedEntity, fcm_registration->authorized_entity);
  EXPECT_EQ(time_now, fcm_registration->timestamp);

  // Set FCM registration without authorized entity.
  sharing_sync_preference_.SetFCMRegistration(
      SharingSyncPreference::FCMRegistration(std::nullopt, time_now));

  fcm_registration = sharing_sync_preference_.GetFCMRegistration();
  EXPECT_TRUE(fcm_registration);
  EXPECT_FALSE(fcm_registration->authorized_entity);
  EXPECT_EQ(time_now, fcm_registration->timestamp);

  sharing_sync_preference_.ClearFCMRegistration();
  EXPECT_FALSE(sharing_sync_preference_.GetFCMRegistration());
}

TEST_F(SharingSyncPreferenceTest, GetLocalSharingInfoForSync) {
  auto sharing_info = GetDefaultSharingInfo();
  sharing_sync_preference_.SetLocalSharingInfo(sharing_info);

  EXPECT_EQ(sharing_info,
            SharingSyncPreference::GetLocalSharingInfoForSync(&prefs_));
}

TEST_F(SharingSyncPreferenceTest, GetLocalSharingInfoForSync_InvalidEnum) {
  auto sharing_info = GetDefaultSharingInfo();
  sharing_sync_preference_.SetLocalSharingInfo(sharing_info);

  // Add invalid enabled feature enum value.
  AddEnabledFeature(/*feature=*/-1);

  // Expect invalid enum value to be filtered out.
  EXPECT_EQ(sharing_info,
            SharingSyncPreference::GetLocalSharingInfoForSync(&prefs_));
}
