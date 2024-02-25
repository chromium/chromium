// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/multidevice/remote_device.h"

#include "base/base64.h"

namespace ash::multidevice {

// static
std::string RemoteDevice::GenerateDeviceId(const std::string& public_key) {
  return base::Base64Encode(public_key);
}

// static
std::string RemoteDevice::DerivePublicKey(const std::string& device_id) {
  std::string public_key;
  if (base::Base64Decode(device_id, &public_key))
    return public_key;
  return std::string();
}

RemoteDevice::RemoteDevice() : last_update_time_millis(0L) {}

RemoteDevice::RemoteDevice(
    const std::string& user_email,
    const std::string& instance_id,
    const std::string& name,
    const std::string& pii_free_name,
    const std::string& public_key,
    const std::string& persistent_symmetric_key,
    int64_t last_update_time_millis,
    const std::map<SoftwareFeature, SoftwareFeatureState>& software_features,
    const std::vector<BeaconSeed>& beacon_seeds,
    const std::string& bluetooth_public_address)
    : user_email(user_email),
      instance_id(instance_id),
      name(name),
      pii_free_name(pii_free_name),
      public_key(public_key),
      persistent_symmetric_key(persistent_symmetric_key),
      last_update_time_millis(last_update_time_millis),
      software_features(software_features),
      beacon_seeds(beacon_seeds),
      bluetooth_public_address(bluetooth_public_address) {}

RemoteDevice::RemoteDevice(const RemoteDevice& other) = default;

RemoteDevice::~RemoteDevice() {}

std::string RemoteDevice::GetDeviceId() const {
  return RemoteDevice::GenerateDeviceId(public_key);
}

bool RemoteDevice::operator==(const RemoteDevice& other) const {
  return user_email == other.user_email && instance_id == other.instance_id &&
         name == other.name && pii_free_name == other.pii_free_name &&
         public_key == other.public_key &&
         persistent_symmetric_key == other.persistent_symmetric_key &&
         last_update_time_millis == other.last_update_time_millis &&
         software_features == other.software_features &&
         beacon_seeds == other.beacon_seeds &&
         bluetooth_public_address == other.bluetooth_public_address;
}

bool RemoteDevice::operator<(const RemoteDevice& other) const {
  if (!instance_id.empty() || !other.instance_id.empty())
    return instance_id.compare(other.instance_id) < 0;

  // |public_key| can contain null bytes, so use GetDeviceId(), which cannot
  // contain null bytes, to compare devices.
  // Note: Devices that do not have an Instance ID are v1 DeviceSync devices,
  // which should have a public key.
  return GetDeviceId().compare(other.GetDeviceId()) < 0;
}

}  // namespace ash::multidevice
