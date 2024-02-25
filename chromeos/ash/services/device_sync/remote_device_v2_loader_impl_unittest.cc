// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/components/multidevice/fake_secure_message_delegate.h"
#include "chromeos/ash/components/multidevice/remote_device.h"
#include "chromeos/ash/services/device_sync/cryptauth_device.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_v2_test_util.h"
#include "chromeos/ash/services/device_sync/remote_device_v2_loader_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::device_sync {

namespace {

// Prefixes for RemoteDevice fields.
const char kDeviceNamePrefix[] = "device_name";
const char kNoPiiDeviceNamePrefix[] = "no_pii_here";
const char kPublicKeyPrefix[] = "public_key";
const char kInstanceIdPrefix[] = "instance_id";
const char kPskPlaceholder[] = "psk_placeholder";
const char kBluetoothPublicAddressPrefix[] = "bluetooth_public_address";

// The id of the user who the remote devices belong to.
const char kUserId[] = "example@gmail.com";

// The public key of the user's local device.
const char kUserPublicKey[] = "user_public_key";

// BeaconSeed values.
const int64_t kBeaconSeedStartTimeMs = 1000;
const int64_t kBeaconSeedEndTimeMs = 2000;
const char kBeaconSeedData[] = "beacon_seed_data";

// The "DeviceSync:BetterTogether" public key prefix.
const char kDeviceSyncBetterTogetherPublicKeyPrefix[] = "ds_beto_pub_key";

// Last update time in milliseconds
const int64_t kLastUpdateTimeMs = 3000;

// Creates a CryptAuthDevice with |suffix| appended to each predetermined string
// field. If |has_beto_metadata| is false,
// CryptAuthDevice.better_together_device_metadata is not set, and if
// |has_public_key| is false,
// CryptAuthDevice.better_together_device_metadata.public_key is not set.
CryptAuthDevice CreateCryptAuthDevice(const std::string& suffix,
                                      bool has_beto_metadata,
                                      bool has_public_key,
                                      bool has_bluetooth_address) {
  std::optional<cryptauthv2::BetterTogetherDeviceMetadata> beto_metadata;

  if (has_beto_metadata) {
    beto_metadata = cryptauthv2::BetterTogetherDeviceMetadata();
    beto_metadata->set_no_pii_device_name(kNoPiiDeviceNamePrefix + suffix);

    cryptauthv2::BeaconSeed* beacon_seed = beto_metadata->add_beacon_seeds();
    beacon_seed->set_start_time_millis(kBeaconSeedStartTimeMs);
    beacon_seed->set_end_time_millis(kBeaconSeedEndTimeMs);
    beacon_seed->set_data(kBeaconSeedData + suffix);

    if (has_public_key)
      beto_metadata->set_public_key(kPublicKeyPrefix + suffix);

    if (has_bluetooth_address) {
      beto_metadata->set_bluetooth_public_address(
          kBluetoothPublicAddressPrefix + suffix);
    }
  }

  return CryptAuthDevice(
      kInstanceIdPrefix + suffix, kDeviceNamePrefix + suffix,
      kDeviceSyncBetterTogetherPublicKeyPrefix + suffix,
      base::Time::FromMillisecondsSinceUnixEpoch(kLastUpdateTimeMs),
      beto_metadata,
      {{multidevice::SoftwareFeature::kBetterTogetherHost,
        multidevice::SoftwareFeatureState::kSupported}});
}

// Creates a RemoteDevice with |suffix| appended to each predetermined string
// field. If |has_beto_metadata| is false, RemoteDevice.pii_free_name is not
// set. If |has_public_key| is false, RemoteDevice.public_key and
// RemoteDevice.persistent_symmetric_key is not set. If |has_beacon_seeds| is
// false, RemoteDevice.beacon_seeds is not set.
multidevice::RemoteDevice CreateRemoteDevice(const std::string& suffix,
                                             bool has_pii_free_name,
                                             bool has_public_key,
                                             bool has_beacon_seeds,
                                             bool has_bluetooth_address) {
  std::string pii_free_name =
      has_pii_free_name ? kNoPiiDeviceNamePrefix + suffix : std::string();
  std::string public_key =
      has_public_key ? kPublicKeyPrefix + suffix : std::string();
  std::string psk = has_public_key ? kPskPlaceholder : std::string();
  std::string bluetooth_address = has_bluetooth_address
                                      ? kBluetoothPublicAddressPrefix + suffix
                                      : std::string();

  std::vector<multidevice::BeaconSeed> beacon_seeds;
  if (has_beacon_seeds) {
    beacon_seeds.emplace_back(
        kBeaconSeedData + suffix,
        base::Time::FromMillisecondsSinceUnixEpoch(kBeaconSeedStartTimeMs),
        base::Time::FromMillisecondsSinceUnixEpoch(kBeaconSeedEndTimeMs));
  }

  return multidevice::RemoteDevice(
      kUserId, kInstanceIdPrefix + suffix, kDeviceNamePrefix + suffix,
      pii_free_name, public_key, psk, kLastUpdateTimeMs,
      {{multidevice::SoftwareFeature::kBetterTogetherHost,
        multidevice::SoftwareFeatureState::kSupported}},
      beacon_seeds, bluetooth_address);
}

}  // namespace

class DeviceSyncRemoteDeviceV2LoaderImplTest : public testing::Test {
 public:
  DeviceSyncRemoteDeviceV2LoaderImplTest()
      : fake_secure_message_delegate_factory_(
            std::make_unique<multidevice::FakeSecureMessageDelegateFactory>()),
        user_private_key_(fake_secure_message_delegate_factory_->instance()
                              ->GetPrivateKeyForPublicKey(kUserPublicKey)) {}

  ~DeviceSyncRemoteDeviceV2LoaderImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    multidevice::SecureMessageDelegateImpl::Factory::SetFactoryForTesting(
        fake_secure_message_delegate_factory_.get());
  }

  // testing::Test:
  void TearDown() override {
    multidevice::SecureMessageDelegateImpl::Factory::SetFactoryForTesting(
        nullptr);
  }

  void CallLoad(std::vector<CryptAuthDevice> device_list) {
    CryptAuthDeviceRegistry::InstanceIdToDeviceMap id_to_device_map;
    for (const auto& device : device_list)
      id_to_device_map.insert_or_assign(device.instance_id(), device);

    loader_ = RemoteDeviceV2LoaderImpl::Factory::Create();
    loader_->Load(
        id_to_device_map, kUserId, user_private_key_,
        base::BindOnce(&DeviceSyncRemoteDeviceV2LoaderImplTest::OnLoadFinished,
                       base::Unretained(this)));
  }

  void OnLoadFinished(const multidevice::RemoteDeviceList& remote_devices) {
    remote_devices_ = remote_devices;
  }

  void VerifyLoad(
      const multidevice::RemoteDeviceList& expected_remote_devices) {
    ASSERT_TRUE(remote_devices_);
    EXPECT_EQ(expected_remote_devices.size(), remote_devices_->size());

    for (const auto& expected_device : expected_remote_devices) {
      std::string expected_instance_id = expected_device.instance_id;
      auto it = base::ranges::find(*remote_devices_, expected_instance_id,
                                   &multidevice::RemoteDevice::instance_id);

      ASSERT_FALSE(it == remote_devices_->end());
      multidevice::RemoteDevice remote_device = *it;

      // Because of the way FakeSecureMessageDelegate is implemented, we do not
      // know the form of the derived persistent symmetric key, only if it is
      // empty or not. After checking if both the actual and expected keys are
      // empty or not empty, replace the placeholder PSK with the PSK of the
      // expected RemoteDevice. This allows us to directly compare two
      // RemoteDevice objects.
      EXPECT_EQ(expected_device.persistent_symmetric_key.empty(),
                remote_device.persistent_symmetric_key.empty());
      remote_device.persistent_symmetric_key =
          expected_device.persistent_symmetric_key;

      EXPECT_EQ(expected_device, remote_device);
    }
  }

 protected:
  // Null until Load() finishes.
  std::optional<multidevice::RemoteDeviceList> remote_devices_;

  std::unique_ptr<multidevice::FakeSecureMessageDelegateFactory>
      fake_secure_message_delegate_factory_;
  std::string user_private_key_;
  std::unique_ptr<RemoteDeviceV2Loader> loader_;
};

TEST_F(DeviceSyncRemoteDeviceV2LoaderImplTest, NoDevices) {
  CallLoad({} /* device_list */);
  VerifyLoad({} /* expected_remote_devices */);
}

TEST_F(DeviceSyncRemoteDeviceV2LoaderImplTest, Success) {
  CallLoad({CreateCryptAuthDevice("device1", true /* has_beto_metadata */,
                                  true /* has_public_key */,
                                  true /* has_bluetooth_address */),
            CreateCryptAuthDevice("device2", true /* has_beto_metadata */,
                                  false /* has_public_key */,
                                  false /* has_bluetooth_address */),
            CreateCryptAuthDevice("device3", false /* has_beto_metadata */,
                                  false /* has_public_key */,
                                  false /* has_bluetooth_address */)});

  VerifyLoad(
      {CreateRemoteDevice(
           "device1", true /* has_pii_free_name */, true /* has_public_key */,
           true /* has_beacon_seeds */, true /* has_bluetooth_address */),
       CreateRemoteDevice(
           "device2", true /* has_pii_free_name */, false /* has_public_key */,
           true /* has_beacon_seeds */, false /* has_bluetooth_address */),
       CreateRemoteDevice(
           "device3", false /* has_pii_free_name */, false /* has_public_key */,
           false /* has_beacon_seeds */, false /* has_bluetooth_address */)});
}

}  // namespace ash::device_sync
