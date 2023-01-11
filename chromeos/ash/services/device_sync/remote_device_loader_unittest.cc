// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/remote_device_loader.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chromeos/ash/components/multidevice/fake_secure_message_delegate.h"
#include "chromeos/ash/services/device_sync/proto/enum_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace device_sync {

namespace {

// Prefixes for RemoteDevice fields.
const char kDeviceNamePrefix[] = "device";
const char kNoPiiDeviceNamePrefix[] = "no pii here";
const char kPublicKeyPrefix[] = "pk";

// The id of the user who the remote devices belong to.
const char kUserId[] = "example@gmail.com";

// The public key of the user's local device.
const char kUserPublicKey[] = "User public key";

// BeaconSeed values.
const int64_t kBeaconSeedStartTimeMs = 1000;
const int64_t kBeaconSeedEndTimeMs = 2000;
const char kBeaconSeedData[] = "Beacon Seed Data";

// Creates and returns an cryptauth::ExternalDeviceInfo proto with the fields
// appended with |suffix|.
cryptauth::ExternalDeviceInfo CreateDeviceInfo(const std::string& suffix) {
  cryptauth::ExternalDeviceInfo device_info;
  device_info.set_friendly_device_name(std::string(kDeviceNamePrefix) + suffix);
  device_info.set_no_pii_device_name(std::string(kNoPiiDeviceNamePrefix) +
                                     suffix);
  device_info.set_public_key(std::string(kPublicKeyPrefix) + suffix);
  device_info.add_beacon_seeds();
  cryptauth::BeaconSeed* beacon_seed = device_info.mutable_beacon_seeds(0);
  beacon_seed->set_start_time_millis(kBeaconSeedStartTimeMs);
  beacon_seed->set_end_time_millis(kBeaconSeedEndTimeMs);
  beacon_seed->set_data(kBeaconSeedData);
  return device_info;
}

}  // namespace

class DeviceSyncRemoteDeviceLoaderTest : public testing::Test {
 public:
  DeviceSyncRemoteDeviceLoaderTest()
      : secure_message_delegate_(new multidevice::FakeSecureMessageDelegate()),
        user_private_key_(secure_message_delegate_->GetPrivateKeyForPublicKey(
            kUserPublicKey)) {}

  DeviceSyncRemoteDeviceLoaderTest(const DeviceSyncRemoteDeviceLoaderTest&) =
      delete;
  DeviceSyncRemoteDeviceLoaderTest& operator=(
      const DeviceSyncRemoteDeviceLoaderTest&) = delete;

  ~DeviceSyncRemoteDeviceLoaderTest() override {}

  void OnRemoteDevicesLoaded(
      const multidevice::RemoteDeviceList& remote_devices) {
    remote_devices_ = remote_devices;
    LoadCompleted();
  }

  MOCK_METHOD0(LoadCompleted, void());

 protected:
  // Handles deriving the PSK. Ownership will be passed to the\
  // RemoteDeviceLoader under test.
  std::unique_ptr<multidevice::FakeSecureMessageDelegate>
      secure_message_delegate_;

  // The private key of the user local device.
  std::string user_private_key_;

  // Stores the result of the RemoteDeviceLoader.
  multidevice::RemoteDeviceList remote_devices_;
};

TEST_F(DeviceSyncRemoteDeviceLoaderTest, LoadZeroDevices) {
  std::vector<cryptauth::ExternalDeviceInfo> device_infos;
  RemoteDeviceLoader loader(device_infos, user_private_key_, kUserId,
                            std::move(secure_message_delegate_));

  EXPECT_CALL(*this, LoadCompleted());
  loader.Load(
      base::BindOnce(&DeviceSyncRemoteDeviceLoaderTest::OnRemoteDevicesLoaded,
                     base::Unretained(this)));

  EXPECT_EQ(0u, remote_devices_.size());
}

TEST_F(DeviceSyncRemoteDeviceLoaderTest, LoadOneDevice) {
  std::vector<cryptauth::ExternalDeviceInfo> device_infos(
      1, CreateDeviceInfo("0"));
  RemoteDeviceLoader loader(device_infos, user_private_key_, kUserId,
                            std::move(secure_message_delegate_));

  EXPECT_CALL(*this, LoadCompleted());
  loader.Load(
      base::BindOnce(&DeviceSyncRemoteDeviceLoaderTest::OnRemoteDevicesLoaded,
                     base::Unretained(this)));

  EXPECT_EQ(1u, remote_devices_.size());
  EXPECT_FALSE(remote_devices_[0].persistent_symmetric_key.empty());
  EXPECT_EQ(device_infos[0].friendly_device_name(), remote_devices_[0].name);
  EXPECT_EQ(device_infos[0].no_pii_device_name(),
            remote_devices_[0].pii_free_name);
  EXPECT_EQ(device_infos[0].public_key(), remote_devices_[0].public_key);
  ASSERT_EQ(1u, remote_devices_[0].beacon_seeds.size());

  const cryptauth::BeaconSeed& beacon_seed =
      multidevice::ToCryptAuthSeed(remote_devices_[0].beacon_seeds[0]);
  EXPECT_EQ(kBeaconSeedData, beacon_seed.data());
  EXPECT_EQ(kBeaconSeedStartTimeMs, beacon_seed.start_time_millis());
  EXPECT_EQ(kBeaconSeedEndTimeMs, beacon_seed.end_time_millis());
}

TEST_F(DeviceSyncRemoteDeviceLoaderTest, LastUpdateTimeMillis) {
  cryptauth::ExternalDeviceInfo first = CreateDeviceInfo("0");
  first.set_last_update_time_millis(1000);

  cryptauth::ExternalDeviceInfo second = CreateDeviceInfo("1");
  second.set_last_update_time_millis(2000);

  std::vector<cryptauth::ExternalDeviceInfo> device_infos{first, second};

  RemoteDeviceLoader loader(device_infos, user_private_key_, kUserId,
                            std::move(secure_message_delegate_));

  EXPECT_CALL(*this, LoadCompleted());
  loader.Load(
      base::BindOnce(&DeviceSyncRemoteDeviceLoaderTest::OnRemoteDevicesLoaded,
                     base::Unretained(this)));

  EXPECT_EQ(2u, remote_devices_.size());

  EXPECT_EQ(1000, remote_devices_[0].last_update_time_millis);

  EXPECT_EQ(2000, remote_devices_[1].last_update_time_millis);
}

TEST_F(DeviceSyncRemoteDeviceLoaderTest, SoftwareFeatures) {
  const std::vector<cryptauth::SoftwareFeature> kSupportedSoftwareFeatures{
      cryptauth::BETTER_TOGETHER_HOST, cryptauth::BETTER_TOGETHER_CLIENT};
  const std::vector<cryptauth::SoftwareFeature> kEnabledSoftwareFeatures{
      cryptauth::BETTER_TOGETHER_HOST};

  cryptauth::ExternalDeviceInfo first = CreateDeviceInfo("0");
  for (const auto& software_feature : kSupportedSoftwareFeatures) {
    first.add_supported_software_features(
        SoftwareFeatureEnumToString(software_feature));
  }
  for (const auto& software_feature : kEnabledSoftwareFeatures) {
    first.add_enabled_software_features(
        SoftwareFeatureEnumToString(software_feature));
  }

  std::vector<cryptauth::ExternalDeviceInfo> device_infos{first};

  RemoteDeviceLoader loader(device_infos, user_private_key_, kUserId,
                            std::move(secure_message_delegate_));

  EXPECT_CALL(*this, LoadCompleted());
  loader.Load(
      base::BindOnce(&DeviceSyncRemoteDeviceLoaderTest::OnRemoteDevicesLoaded,
                     base::Unretained(this)));

  EXPECT_EQ(1u, remote_devices_.size());

  EXPECT_EQ(multidevice::SoftwareFeatureState::kSupported,
            remote_devices_[0].software_features
                [multidevice::SoftwareFeature::kBetterTogetherClient]);
  EXPECT_EQ(multidevice::SoftwareFeatureState::kEnabled,
            remote_devices_[0].software_features
                [multidevice::SoftwareFeature::kBetterTogetherHost]);
  EXPECT_EQ(multidevice::SoftwareFeatureState::kNotSupported,
            remote_devices_[0].software_features
                [multidevice::SoftwareFeature::kInstantTetheringHost]);
}

}  // namespace device_sync

}  // namespace ash
