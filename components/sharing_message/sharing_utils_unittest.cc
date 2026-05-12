// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_utils.h"

#include "base/test/scoped_feature_list.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/test_device_info_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kDeviceGuid[] = "test_guid";
const char kDeviceName[] = "test_name";
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
  std::unique_ptr<syncer::DeviceInfo> device_info =
      syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kLinux)
          .WithGuid(kDeviceGuid)
          .WithClientName(kDeviceName)
          .WithSharingInfo(
              {{kSenderIdFCMToken, kSenderIdP256dh, kSenderIdAuthSecret},
               /*chime_representative_target_id=*/std::string(),
               std::set<syncer::DeviceInfo::SharingFeature>()})
          .Build();

  auto fcm_channel = GetFCMChannel(*device_info);

  ASSERT_TRUE(fcm_channel);
  EXPECT_EQ(fcm_channel->sender_id_fcm_token(), kSenderIdFCMToken);
  EXPECT_EQ(fcm_channel->sender_id_p256dh(), kSenderIdP256dh);
  EXPECT_EQ(fcm_channel->sender_id_auth_secret(), kSenderIdAuthSecret);
}

TEST_F(SharingUtilsTest, GetDevicePlatform) {
  EXPECT_EQ(GetDevicePlatform(*syncer::TestDeviceInfoBuilder(
                                   syncer::DeviceInfo::OsType::kChromeOsAsh)
                                   .WithGuid(kDeviceGuid)
                                   .WithClientName(kDeviceName)
                                   .Build()),
            SharingDevicePlatform::kChromeOS);

  EXPECT_EQ(GetDevicePlatform(*syncer::TestDeviceInfoBuilder(
                                   syncer::DeviceInfo::OsType::kLinux)
                                   .WithGuid(kDeviceGuid)
                                   .WithClientName(kDeviceName)
                                   .Build()),
            SharingDevicePlatform::kLinux);

  EXPECT_EQ(GetDevicePlatform(
                *syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kMac)
                     .WithGuid(kDeviceGuid)
                     .WithClientName(kDeviceName)
                     .Build()),
            SharingDevicePlatform::kMac);

  EXPECT_EQ(GetDevicePlatform(*syncer::TestDeviceInfoBuilder(
                                   syncer::DeviceInfo::OsType::kWindows)
                                   .WithGuid(kDeviceGuid)
                                   .WithClientName(kDeviceName)
                                   .Build()),
            SharingDevicePlatform::kWindows);

  EXPECT_EQ(GetDevicePlatform(
                *syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kIOS)
                     .WithGuid(kDeviceGuid)
                     .WithClientName(kDeviceName)
                     .Build()),
            SharingDevicePlatform::kIOS);
  EXPECT_EQ(GetDevicePlatform(
                *syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kIOS)
                     .WithGuid(kDeviceGuid)
                     .WithClientName(kDeviceName)
                     .WithFormFactor(syncer::DeviceInfo::FormFactor::kTablet)
                     .Build()),
            SharingDevicePlatform::kIOS);

  EXPECT_EQ(GetDevicePlatform(*syncer::TestDeviceInfoBuilder(
                                   syncer::DeviceInfo::OsType::kAndroid)
                                   .WithGuid(kDeviceGuid)
                                   .WithClientName(kDeviceName)
                                   .Build()),
            SharingDevicePlatform::kAndroid);
  EXPECT_EQ(
      GetDevicePlatform(
          *syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kAndroid)
               .WithGuid(kDeviceGuid)
               .WithClientName(kDeviceName)
               .WithFormFactor(syncer::DeviceInfo::FormFactor::kTablet)
               .Build()),
      SharingDevicePlatform::kAndroid);

  EXPECT_EQ(GetDevicePlatform(*syncer::TestDeviceInfoBuilder(
                                   syncer::DeviceInfo::OsType::kUnknown)
                                   .WithGuid(kDeviceGuid)
                                   .WithClientName(kDeviceName)
                                   .Build()),
            SharingDevicePlatform::kUnknown);
  EXPECT_EQ(GetDevicePlatform(*syncer::TestDeviceInfoBuilder(
                                   syncer::DeviceInfo::OsType::kUnknown)
                                   .WithGuid(kDeviceGuid)
                                   .WithClientName(kDeviceName)
                                   .Build()),
            SharingDevicePlatform::kUnknown);
}
