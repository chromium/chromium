// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/public/mojom/device_sync_mojom_traits.h"

#include "chromeos/ash/services/device_sync/feature_status_change.h"
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

    ash::device_sync::mojom::ConnectivityStatus serialized_status =
        mojo::EnumTraits<ash::device_sync::mojom::ConnectivityStatus,
                         cryptauthv2::ConnectivityStatus>::ToMojom(status_in);
    ASSERT_TRUE((mojo::EnumTraits<
                 ash::device_sync::mojom::ConnectivityStatus,
                 cryptauthv2::ConnectivityStatus>::FromMojom(serialized_status,
                                                             &status_out)));
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

    ash::device_sync::mojom::FeatureStatusChange serialized_status =
        mojo::EnumTraits<
            ash::device_sync::mojom::FeatureStatusChange,
            ash::device_sync::FeatureStatusChange>::ToMojom(status_in);
    ASSERT_TRUE((mojo::EnumTraits<ash::device_sync::mojom::FeatureStatusChange,
                                  ash::device_sync::FeatureStatusChange>::
                     FromMojom(serialized_status, &status_out)));
    EXPECT_EQ(status_in, status_out);
  }
}

TEST(DeviceSyncMojomTraitsTest, TargetService) {
  static constexpr cryptauthv2::TargetService kTestTargetServices[] = {
      cryptauthv2::TargetService::ENROLLMENT,
      cryptauthv2::TargetService::DEVICE_SYNC};

  for (auto status_in : kTestTargetServices) {
    cryptauthv2::TargetService status_out;

    ash::device_sync::mojom::CryptAuthService serialized_status =
        mojo::EnumTraits<ash::device_sync::mojom::CryptAuthService,
                         cryptauthv2::TargetService>::ToMojom(status_in);
    ASSERT_TRUE((mojo::EnumTraits<
                 ash::device_sync::mojom::CryptAuthService,
                 cryptauthv2::TargetService>::FromMojom(serialized_status,
                                                        &status_out)));
    EXPECT_EQ(status_in, status_out);
  }
}
