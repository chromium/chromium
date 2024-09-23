// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_utils.h"

#include "base/test/scoped_feature_list.h"
#include "components/sharing_message/fake_device_info.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kDeviceGuid[] = "test_guid";
const char kDeviceName[] = "test_name";
const char kVapidFCMToken[] = "test_vapid_fcm_token";
const char kVapidP256dh[] = "test_vapid_p256_dh";
const char kVapidAuthSecret[] = "test_vapid_auth_secret";
const char kSenderIdFCMToken[] = "test_sender_id_fcm_token";
const char kSenderIdP256dh[] = "test_sender_id_p256_dh";
const char kSenderIdAuthSecret[] = "test_sender_id_auth_secret";

class SharingUtilsTest : public testing::Test {
 public:
  SharingUtilsTest() = default;

 protected:
  syncer::TestSyncService test_sync_service_;
};

}  // namespace

TEST_F(SharingUtilsTest, SyncEnabled_FullySynced) {
  // PREFERENCES is actively synced.
  test_sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPreferences});

  EXPECT_TRUE(IsSyncEnabledForSharing(&test_sync_service_));
  EXPECT_FALSE(IsSyncDisabledForSharing(&test_sync_service_));
}

TEST_F(SharingUtilsTest, SyncDisabled_FullySynced_MissingDataTypes) {
  // Missing PREFERENCES.
  test_sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());
  // Not able to sync SHARING_MESSAGE.
  test_sync_service_.SetFailedDataTypes({syncer::SHARING_MESSAGE});

  EXPECT_FALSE(IsSyncEnabledForSharing(&test_sync_service_));
  EXPECT_TRUE(IsSyncDisabledForSharing(&test_sync_service_));
}

TEST_F(SharingUtilsTest, SyncEnabled_SigninOnly) {
  // SHARING_MESSAGE is actively synced.
  EXPECT_TRUE(IsSyncEnabledForSharing(&test_sync_service_));
  EXPECT_FALSE(IsSyncDisabledForSharing(&test_sync_service_));
}

TEST_F(SharingUtilsTest, SyncDisabled_SigninOnly_MissingDataTypes) {
  // Missing SHARING_MESSAGE.
  test_sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());
  test_sync_service_.SetFailedDataTypes({syncer::SHARING_MESSAGE});

  EXPECT_FALSE(IsSyncEnabledForSharing(&test_sync_service_));
  EXPECT_TRUE(IsSyncDisabledForSharing(&test_sync_service_));
}

TEST_F(SharingUtilsTest, SyncDisabled_Disabled) {
  test_sync_service_.SetSignedOut();
  test_sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPreferences});

  EXPECT_FALSE(IsSyncEnabledForSharing(&test_sync_service_));
  EXPECT_TRUE(IsSyncDisabledForSharing(&test_sync_service_));
}

TEST_F(SharingUtilsTest, SyncDisabled_Configuring) {
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::CONFIGURING);
  test_sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPreferences});

  EXPECT_FALSE(IsSyncEnabledForSharing(&test_sync_service_));
  EXPECT_FALSE(IsSyncDisabledForSharing(&test_sync_service_));
}

TEST_F(SharingUtilsTest, GetFCMChannel) {
  std::unique_ptr<syncer::DeviceInfo> device_info = CreateFakeDeviceInfo(
      kDeviceGuid, kDeviceName,
      syncer::DeviceInfo::SharingInfo(
          {kVapidFCMToken, kVapidP256dh, kVapidAuthSecret},
          {kSenderIdFCMToken, kSenderIdP256dh, kSenderIdAuthSecret},
          /*chime_representative_target_id=*/std::string(),
          std::set<sync_pb::SharingSpecificFields::EnabledFeatures>()));

  auto fcm_channel = GetFCMChannel(*device_info);

  ASSERT_TRUE(fcm_channel);
  EXPECT_EQ(fcm_channel->vapid_fcm_token(), kVapidFCMToken);
  EXPECT_EQ(fcm_channel->vapid_p256dh(), kVapidP256dh);
  EXPECT_EQ(fcm_channel->vapid_auth_secret(), kVapidAuthSecret);
  EXPECT_EQ(fcm_channel->sender_id_fcm_token(), kSenderIdFCMToken);
  EXPECT_EQ(fcm_channel->sender_id_p256dh(), kSenderIdP256dh);
  EXPECT_EQ(fcm_channel->sender_id_auth_secret(), kSenderIdAuthSecret);
}

TEST_F(SharingUtilsTest, GetDevicePlatform) {
  EXPECT_EQ(GetDevicePlatform(*CreateFakeDeviceInfo(
                kDeviceGuid, kDeviceName, /*sharing_info=*/std::nullopt,
                sync_pb::SyncEnums_DeviceType_TYPE_CROS,
                syncer::DeviceInfo::OsType::kChromeOsAsh,
                syncer::DeviceInfo::FormFactor::kDesktop)),
            SharingDevicePlatform::kChromeOS);

  EXPECT_EQ(GetDevicePlatform(*CreateFakeDeviceInfo(
                kDeviceGuid, kDeviceName, /*sharing_info=*/std::nullopt,
                sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
                syncer::DeviceInfo::OsType::kLinux,
                syncer::DeviceInfo::FormFactor::kDesktop)),
            SharingDevicePlatform::kLinux);

  EXPECT_EQ(GetDevicePlatform(*CreateFakeDeviceInfo(
                kDeviceGuid, kDeviceName, /*sharing_info=*/std::nullopt,
                sync_pb::SyncEnums_DeviceType_TYPE_MAC,
                syncer::DeviceInfo::OsType::kMac,
                syncer::DeviceInfo::FormFactor::kDesktop)),
            SharingDevicePlatform::kMac);

  EXPECT_EQ(GetDevicePlatform(*CreateFakeDeviceInfo(
                kDeviceGuid, kDeviceName, /*sharing_info=*/std::nullopt,
                sync_pb::SyncEnums_DeviceType_TYPE_WIN,
                syncer::DeviceInfo::OsType::kWindows,
                syncer::DeviceInfo::FormFactor::kDesktop)),
            SharingDevicePlatform::kWindows);

  EXPECT_EQ(
      GetDevicePlatform(*CreateFakeDeviceInfo(
          kDeviceGuid, kDeviceName, /*sharing_info=*/std::nullopt,
          sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
          syncer::DeviceInfo::OsType::kIOS,
          syncer::DeviceInfo::FormFactor::kPhone, "Apple Inc.", "iPhone 50")),
      SharingDevicePlatform::kIOS);
  EXPECT_EQ(
      GetDevicePlatform(*CreateFakeDeviceInfo(
          kDeviceGuid, kDeviceName, /*sharing_info=*/std::nullopt,
          sync_pb::SyncEnums_DeviceType_TYPE_TABLET,
          syncer::DeviceInfo::OsType::kIOS,
          syncer::DeviceInfo::FormFactor::kTablet, "Apple Inc.", "iPad 99")),
      SharingDevicePlatform::kIOS);

  EXPECT_EQ(GetDevicePlatform(*CreateFakeDeviceInfo(
                kDeviceGuid, kDeviceName, /*sharing_info=*/std::nullopt,
                sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
                syncer::DeviceInfo::OsType::kAndroid,
                syncer::DeviceInfo::FormFactor::kPhone, "Google", "Pixel 777")),
            SharingDevicePlatform::kAndroid);
  EXPECT_EQ(GetDevicePlatform(*CreateFakeDeviceInfo(
                kDeviceGuid, kDeviceName, /*sharing_info=*/std::nullopt,
                sync_pb::SyncEnums_DeviceType_TYPE_TABLET,
                syncer::DeviceInfo::OsType::kAndroid,
                syncer::DeviceInfo::FormFactor::kTablet, "Google", "Pixel Z")),
            SharingDevicePlatform::kAndroid);

  EXPECT_EQ(GetDevicePlatform(*CreateFakeDeviceInfo(
                kDeviceGuid, kDeviceName, /*sharing_info=*/std::nullopt,
                sync_pb::SyncEnums_DeviceType_TYPE_UNSET,
                syncer::DeviceInfo::OsType::kUnknown,
                syncer::DeviceInfo::FormFactor::kUnknown)),
            SharingDevicePlatform::kUnknown);
  EXPECT_EQ(GetDevicePlatform(*CreateFakeDeviceInfo(
                kDeviceGuid, kDeviceName, /*sharing_info=*/std::nullopt,
                sync_pb::SyncEnums_DeviceType_TYPE_OTHER,
                syncer::DeviceInfo::OsType::kUnknown,
                syncer::DeviceInfo::FormFactor::kUnknown)),
            SharingDevicePlatform::kUnknown);
}
