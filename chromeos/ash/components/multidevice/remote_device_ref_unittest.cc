// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/multidevice/remote_device_ref.h"

#include <memory>

#include "chromeos/ash/components/multidevice/remote_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::multidevice {

namespace {

const char kFakeBluetoothPublicAddress[] = "01:23:45:67:89:AB";

}  // namespace

class RemoteDeviceRefTest : public testing::Test {
 public:
  RemoteDeviceRefTest(const RemoteDeviceRefTest&) = delete;
  RemoteDeviceRefTest& operator=(const RemoteDeviceRefTest&) = delete;

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
        "user_email", "instance_id", "name", "pii_free_name", "public_key",
        "persistent_symmetric_key", 42000 /* last_update_time_millis */,
        software_feature_to_state_map /* software_features */,
        beacon_seeds /* beacon_seeds */,
        kFakeBluetoothPublicAddress /* bluetooth_public_address */);
  }

  std::shared_ptr<RemoteDevice> remote_device_;
};

TEST_F(RemoteDeviceRefTest, TestFields) {
  RemoteDeviceRef remote_device_ref(remote_device_);

  EXPECT_EQ(remote_device_->user_email, remote_device_ref.user_email());
  EXPECT_EQ(remote_device_->instance_id, remote_device_ref.instance_id());
  EXPECT_EQ(remote_device_->name, remote_device_ref.name());
  EXPECT_EQ(remote_device_->pii_free_name, remote_device_ref.pii_free_name());
  EXPECT_EQ(remote_device_->public_key, remote_device_ref.public_key());
  EXPECT_EQ(remote_device_->persistent_symmetric_key,
            remote_device_ref.persistent_symmetric_key());
  EXPECT_EQ(remote_device_->last_update_time_millis,
            remote_device_ref.last_update_time_millis());
  EXPECT_EQ(&remote_device_->beacon_seeds, &remote_device_ref.beacon_seeds());
  EXPECT_EQ(kFakeBluetoothPublicAddress,
            remote_device_ref.bluetooth_public_address());

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

}  // namespace ash::multidevice
