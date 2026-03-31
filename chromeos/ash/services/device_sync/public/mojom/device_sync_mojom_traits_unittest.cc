// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/public/mojom/device_sync_mojom_traits.h"

#include "chromeos/ash/services/device_sync/feature_status_change.h"
#include "chromeos/ash/services/device_sync/group_private_key_and_better_together_metadata_status.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/ash/services/device_sync/public/mojom/device_sync.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(DeviceSyncMojomTraitsTest, ConnectivityStatus) {
  static constexpr cryptauthv2::ConnectivityStatus kTestConnectivityStatuses[] =
      {cryptauthv2::ConnectivityStatus::ONLINE,
       cryptauthv2::ConnectivityStatus::OFFLINE};

  for (auto status_in : kTestConnectivityStatuses) {
    cryptauthv2::ConnectivityStatus status_out;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
                ash::device_sync::mojom::ConnectivityStatus>(status_in,
                                                             status_out));
    EXPECT_EQ(status_in, status_out);
  }
}

TEST(DeviceSyncMojomTraitsTest, GroupPrivateKeyStatus) {
  static constexpr ash::device_sync::GroupPrivateKeyStatus
      kTestGroupPrivateKeyStatuses[] = {
          ash::device_sync::GroupPrivateKeyStatus::
              kStatusUnavailableBecauseDeviceSyncIsNotInitialized,
          ash::device_sync::GroupPrivateKeyStatus::
              kStatusUnavailableBecauseNoDeviceSyncerSet,
          ash::device_sync::GroupPrivateKeyStatus::kWaitingForGroupPrivateKey,
          ash::device_sync::GroupPrivateKeyStatus::
              kNoEncryptedGroupPrivateKeyReceived,
          ash::device_sync::GroupPrivateKeyStatus::
              kEncryptedGroupPrivateKeyEmpty,
          ash::device_sync::GroupPrivateKeyStatus::
              kLocalDeviceSyncBetterTogetherKeyMissing,
          ash::device_sync::GroupPrivateKeyStatus::
              kGroupPrivateKeyDecryptionFailed,
          ash::device_sync::GroupPrivateKeyStatus::
              kGroupPrivateKeySuccessfullyDecrypted};

  for (auto status_in : kTestGroupPrivateKeyStatuses) {
    ash::device_sync::GroupPrivateKeyStatus status_out;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
                ash::device_sync::mojom::GroupPrivateKeyStatus>(status_in,
                                                                status_out));
    EXPECT_EQ(status_in, status_out);
  }
}

TEST(DeviceSyncMojomTraitsTest, BetterTogetherMetadataStatus) {
  static constexpr ash::device_sync::BetterTogetherMetadataStatus
      kTestBetterTogetherMetadataStatuses[] = {
          ash::device_sync::BetterTogetherMetadataStatus::
              kStatusUnavailableBecauseDeviceSyncIsNotInitialized,
          ash::device_sync::BetterTogetherMetadataStatus::
              kStatusUnavailableBecauseNoDeviceSyncerSet,
          ash::device_sync::BetterTogetherMetadataStatus::
              kWaitingToProcessDeviceMetadata,
          ash::device_sync::BetterTogetherMetadataStatus::
              kGroupPrivateKeyMissing,
          ash::device_sync::BetterTogetherMetadataStatus::
              kEncryptedMetadataEmpty,
          ash::device_sync::BetterTogetherMetadataStatus::kMetadataDecrypted};

  for (auto status_in : kTestBetterTogetherMetadataStatuses) {
    ash::device_sync::BetterTogetherMetadataStatus status_out;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
                ash::device_sync::mojom::BetterTogetherMetadataStatus>(
        status_in, status_out));
    EXPECT_EQ(status_in, status_out);
  }
}

TEST(DeviceSyncMojomTraitsTest, FeatureStatusChange) {
  static constexpr ash::device_sync::FeatureStatusChange
      kTestFeatureStatusChanges[] = {
          ash::device_sync::FeatureStatusChange::kEnableExclusively,
          ash::device_sync::FeatureStatusChange::kEnableNonExclusively,
          ash::device_sync::FeatureStatusChange::kDisable};

  for (auto status_in : kTestFeatureStatusChanges) {
    ash::device_sync::FeatureStatusChange status_out;

    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
                ash::device_sync::mojom::FeatureStatusChange>(status_in,
                                                              status_out));
    EXPECT_EQ(status_in, status_out);
  }
}

TEST(DeviceSyncMojomTraitsTest, TargetService) {
  static constexpr cryptauthv2::TargetService kTestTargetServices[] = {
      cryptauthv2::TargetService::ENROLLMENT,
      cryptauthv2::TargetService::DEVICE_SYNC};

  for (auto status_in : kTestTargetServices) {
    cryptauthv2::TargetService status_out;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<
            ash::device_sync::mojom::CryptAuthService>(status_in, status_out));
    EXPECT_EQ(status_in, status_out);
  }
}
