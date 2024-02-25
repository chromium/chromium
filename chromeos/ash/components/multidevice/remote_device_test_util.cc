// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/multidevice/remote_device_test_util.h"

#include <map>
#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/strings/string_number_conversions.h"

namespace ash::multidevice {

namespace {

// Attributes of the default test remote device.
const char kTestRemoteDeviceUserId[] = "example@gmail.com";
const int64_t kTestRemoteDeviceInstanceId = 0L;
const char kTestRemoteDevicePiiFreeName[] = "no-pii device";
const char kTestRemoteDevicePSK[] = "remote device psk";
const int64_t kTestRemoteDeviceLastUpdateTimeMillis = 0L;
const char kBeaconSeedData[] = "beacon seed data";
const int64_t kBeaconSeedStartTimeMillis = 100L;
const int64_t kBeaconSeedEndTimeMillis = 200L;

// Create an Instance ID, which is a base64 URL-safe encoding of an 8-byte
// integer. This seems like overkill for tests, but some places in code might
// require the specific Instance ID formatting.
std::string InstanceIdFromInt64(int64_t number) {
  // Big-endian representation of |number|.
  uint8_t bytes[sizeof(int64_t)];
  bytes[0] = static_cast<uint8_t>((number >> 56) & 0xff);
  bytes[1] = static_cast<uint8_t>((number >> 48) & 0xff);
  bytes[2] = static_cast<uint8_t>((number >> 40) & 0xff);
  bytes[3] = static_cast<uint8_t>((number >> 32) & 0xff);
  bytes[4] = static_cast<uint8_t>((number >> 24) & 0xff);
  bytes[5] = static_cast<uint8_t>((number >> 16) & 0xff);
  bytes[6] = static_cast<uint8_t>((number >> 8) & 0xff);
  bytes[7] = static_cast<uint8_t>(number & 0xff);

  // Transforms the first 4 bits to 0x7 which is required for Instance IDs.
  bytes[0] &= 0x0f;
  bytes[0] |= 0x70;

  std::string iid;
  base::Base64UrlEncode(
      std::string_view(reinterpret_cast<const char*>(bytes), sizeof(bytes)),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &iid);

  return iid;
}

}  // namespace

// Attributes of the default test remote device.
const char kTestRemoteDeviceName[] = "remote device";
const char kTestRemoteDevicePublicKey[] = "public key";
const char kTestRemoteDeviceBluetoothPublicAddress[] = "01:23:45:67:89:AB";

RemoteDeviceRefBuilder::RemoteDeviceRefBuilder() {
  remote_device_ = std::make_shared<RemoteDevice>(CreateRemoteDeviceForTest());
}

RemoteDeviceRefBuilder::~RemoteDeviceRefBuilder() = default;

RemoteDeviceRefBuilder& RemoteDeviceRefBuilder::SetUserEmail(
    const std::string& user_email) {
  remote_device_->user_email = user_email;
  return *this;
}

RemoteDeviceRefBuilder& RemoteDeviceRefBuilder::SetInstanceId(
    const std::string& instance_id) {
  remote_device_->instance_id = instance_id;
  return *this;
}

RemoteDeviceRefBuilder& RemoteDeviceRefBuilder::SetName(
    const std::string& name) {
  remote_device_->name = name;
  return *this;
}

RemoteDeviceRefBuilder& RemoteDeviceRefBuilder::SetPiiFreeName(
    const std::string& pii_free_name) {
  remote_device_->pii_free_name = pii_free_name;
  return *this;
}

RemoteDeviceRefBuilder& RemoteDeviceRefBuilder::SetPublicKey(
    const std::string& public_key) {
  remote_device_->public_key = public_key;
  return *this;
}

RemoteDeviceRefBuilder& RemoteDeviceRefBuilder::SetSupportsMobileHotspot(
    bool supports_mobile_hotspot) {
  remote_device_->software_features[SoftwareFeature::kInstantTetheringHost] =
      supports_mobile_hotspot ? SoftwareFeatureState::kSupported
                              : SoftwareFeatureState::kNotSupported;
  return *this;
}

RemoteDeviceRefBuilder& RemoteDeviceRefBuilder::SetSoftwareFeatureState(
    const SoftwareFeature feature,
    const SoftwareFeatureState new_state) {
  remote_device_->software_features[feature] = new_state;
  return *this;
}

RemoteDeviceRefBuilder& RemoteDeviceRefBuilder::SetLastUpdateTimeMillis(
    int64_t last_update_time_millis) {
  remote_device_->last_update_time_millis = last_update_time_millis;
  return *this;
}

RemoteDeviceRefBuilder& RemoteDeviceRefBuilder::SetBeaconSeeds(
    const std::vector<BeaconSeed>& beacon_seeds) {
  remote_device_->beacon_seeds = beacon_seeds;
  return *this;
}

RemoteDeviceRefBuilder& RemoteDeviceRefBuilder::SetBluetoothPublicAddress(
    const std::string& bluetooth_public_address) {
  remote_device_->bluetooth_public_address = bluetooth_public_address;
  return *this;
}

RemoteDeviceRef RemoteDeviceRefBuilder::Build() {
  return RemoteDeviceRef(remote_device_);
}

RemoteDevice CreateRemoteDeviceForTest() {
  std::map<SoftwareFeature, SoftwareFeatureState> software_features;
  software_features[SoftwareFeature::kSmartLockHost] =
      SoftwareFeatureState::kEnabled;
  software_features[SoftwareFeature::kInstantTetheringHost] =
      SoftwareFeatureState::kSupported;

  return RemoteDevice(
      kTestRemoteDeviceUserId, InstanceIdFromInt64(kTestRemoteDeviceInstanceId),
      kTestRemoteDeviceName, kTestRemoteDevicePiiFreeName,
      kTestRemoteDevicePublicKey, kTestRemoteDevicePSK,
      kTestRemoteDeviceLastUpdateTimeMillis, software_features,
      {multidevice::BeaconSeed(kBeaconSeedData,
                               base::Time::FromMillisecondsSinceUnixEpoch(
                                   kBeaconSeedStartTimeMillis),
                               base::Time::FromMillisecondsSinceUnixEpoch(
                                   kBeaconSeedEndTimeMillis))},
      kTestRemoteDeviceBluetoothPublicAddress);
}

RemoteDeviceRef CreateRemoteDeviceRefForTest() {
  return RemoteDeviceRefBuilder().Build();
}

RemoteDeviceRefList CreateRemoteDeviceRefListForTest(size_t num_to_create) {
  RemoteDeviceRefList generated_devices;

  for (size_t i = 0; i < num_to_create; i++) {
    RemoteDeviceRef remote_device =
        RemoteDeviceRefBuilder()
            .SetInstanceId(InstanceIdFromInt64(i))
            .SetPublicKey("publicKey" + base::NumberToString(i))
            .Build();
    generated_devices.push_back(remote_device);
  }

  return generated_devices;
}

RemoteDeviceList CreateRemoteDeviceListForTest(size_t num_to_create) {
  RemoteDeviceList generated_devices;

  for (size_t i = 0; i < num_to_create; i++) {
    RemoteDevice remote_device = CreateRemoteDeviceForTest();
    remote_device.instance_id = InstanceIdFromInt64(i);
    remote_device.public_key = "publicKey" + base::NumberToString(i);
    generated_devices.push_back(remote_device);
  }

  return generated_devices;
}

RemoteDevice* GetMutableRemoteDevice(const RemoteDeviceRef& remote_device_ref) {
  const RemoteDevice* remote_device = remote_device_ref.remote_device_.get();
  return const_cast<RemoteDevice*>(remote_device);
}

bool IsSameDevice(const RemoteDevice& remote_device,
                  RemoteDeviceRef remote_device_ref) {
  if (!remote_device_ref.remote_device_)
    return false;

  return remote_device == *remote_device_ref.remote_device_;
}

}  // namespace ash::multidevice
