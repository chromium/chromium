// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_device.h"

#include <map>
#include <optional>

#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_better_together_device_metadata.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_v2_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace device_sync {

namespace {

const char kFakeInstanceId[] = "fake_instance_id";
const char kFakeDeviceName[] = "fake_device_name";
const char kFakeDeviceBetterTogetherPublicKey[] =
    "fake_device_better_together_public_key";

}  // namespace

TEST(DeviceSyncCryptAuthDevice, ToAndFromDictionary) {
  const base::Time kFakeLastUpdateTime =
      base::Time::FromSecondsSinceUnixEpoch(100);
  const std::map<multidevice::SoftwareFeature,
                 multidevice::SoftwareFeatureState>
      kFakeFeatureStates = {
          {multidevice::SoftwareFeature::kBetterTogetherClient,
           multidevice::SoftwareFeatureState::kEnabled},
          {multidevice::SoftwareFeature::kBetterTogetherHost,
           multidevice::SoftwareFeatureState::kNotSupported},
          {multidevice::SoftwareFeature::kMessagesForWebClient,
           multidevice::SoftwareFeatureState::kSupported}};

  CryptAuthDevice expected_device(
      kFakeInstanceId, kFakeDeviceName, kFakeDeviceBetterTogetherPublicKey,
      kFakeLastUpdateTime,
      cryptauthv2::GetBetterTogetherDeviceMetadataForTest(),
      kFakeFeatureStates);

  std::optional<CryptAuthDevice> device =
      CryptAuthDevice::FromDictionary(expected_device.AsDictionary());

  ASSERT_TRUE(device);
  EXPECT_EQ(expected_device, *device);
}

}  // namespace device_sync

}  // namespace ash
