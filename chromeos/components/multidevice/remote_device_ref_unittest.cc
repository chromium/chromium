// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/multidevice/remote_device_ref.h"

#include <memory>

#include "base/macros.h"
#include "chromeos/components/multidevice/remote_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice {

class RemoteDeviceRefTest : public testing::Test {
 protected:
  RemoteDeviceRefTest() = default;

  // testing::Test:
  void SetUp() override {
    std::map<SoftwareFeature, SoftwareFeatureState>
        software_feature_to_state_map;
    software_feature_to_state_map[SoftwareFeature::kBetterTogetherClient] =
        SoftwareFeatureState::kSupported;
    software_feature_to_state_map[SoftwareFeature::kBetterTogetherHost] =
        SoftwareFeatureState::kEnabled;

    std::vector<BeaconSeed> beacon_seeds({BeaconSeed(), BeaconSeed()});

    remote_device_ = std::make_shared<RemoteDevice>(
        "user_id", "instance_id", "name", "pii_free_name", "public_key",
        "persistent_symmetric_key", 42000 /* last_update_time_millis */,
        software_feature_to_state_map /* software_features */,
        beacon_seeds /* beacon_seeds */);
  }

  std::shared_ptr<RemoteDevice> remote_device_;

  DISALLOW_COPY_AND_ASSIGN(RemoteDeviceRefTest);
};

TEST_F(RemoteDeviceRefTest, TestFields) {
  RemoteDeviceRef remote_device_ref(remote_device_);

  EXPECT_EQ(remote_device_->user_id, remote_device_ref.user_id());
  EXPECT_EQ(remote_device_->instance_id, remote_device_ref.instance_id());
  EXPECT_EQ(remote_device_->name, remote_device_ref.name());
  EXPECT_EQ(remote_device_->pii_free_name, remote_device_ref.pii_free_name());
  EXPECT_EQ(remote_device_->public_key, remote_device_ref.public_key());
  EXPECT_EQ(remote_device_->persistent_symmetric_key,
            remote_device_ref.persistent_symmetric_key());
  EXPECT_EQ(remote_device_->last_update_time_millis,
            remote_device_ref.last_update_time_millis());
  EXPECT_EQ(&remote_device_->beacon_seeds, &remote_device_ref.beacon_seeds());

  EXPECT_EQ(SoftwareFeatureState::kNotSupported,
            remote_device_ref.GetSoftwareFeatureState(
                SoftwareFeature::kInstantTetheringClient));
  EXPECT_EQ(SoftwareFeatureState::kSupported,
            remote_device_ref.GetSoftwareFeatureState(
                SoftwareFeature::kBetterTogetherClient));
  EXPECT_EQ(SoftwareFeatureState::kEnabled,
            remote_device_ref.GetSoftwareFeatureState(
                SoftwareFeature::kBetterTogetherHost));

  EXPECT_EQ(remote_device_->GetDeviceId(), remote_device_ref.GetDeviceId());
  EXPECT_EQ(
      RemoteDeviceRef::TruncateDeviceIdForLogs(remote_device_->GetDeviceId()),
      remote_device_ref.GetTruncatedDeviceIdForLogs());
}

TEST_F(RemoteDeviceRefTest, TestCopyAndAssign) {
  RemoteDeviceRef remote_device_ref_1(remote_device_);

  RemoteDeviceRef remote_device_ref_2 = remote_device_ref_1;
  EXPECT_EQ(remote_device_ref_2, remote_device_ref_1);

  RemoteDeviceRef remote_device_ref_3(remote_device_ref_1);
  EXPECT_EQ(remote_device_ref_3, remote_device_ref_1);
}

}  // namespace multidevice

}  // namespace chromeos
