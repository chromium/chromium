// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_REMOTE_DEVICE_H_
#define CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_REMOTE_DEVICE_H_

#include <map>
#include <string>
#include <vector>

#include "chromeos/ash/components/multidevice/beacon_seed.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"

namespace ash::multidevice {

struct RemoteDevice {
  // Generates the device ID for a device given its public key.
  static std::string GenerateDeviceId(const std::string& public_key);

  // Derives the public key that was used to generate the given device ID;
  // returns empty string if |device_id| is not a valid device ID.
  static std::string DerivePublicKey(const std::string& device_id);

  std::string user_email;

  // The Instance ID is the primary identifier for devices using CryptAuth v2,
  // but the Instance ID is not present in CryptAuth v1. This string is empty
  // for devices not using CryptAuth v2 Enrollment and v2 DeviceSync.
  // TODO(crbug.com/40105247): Remove comments when v1 DeviceSync is
  // disabled.
  std::string instance_id;

  std::string name;
  std::string pii_free_name;
  std::string public_key;
  std::string persistent_symmetric_key;

  // The last time a feature bit changed for the device in the CryptAuth server.
  // Beware that devices don't necessarily flip their own bits, for example,
  // exclusively enabling a feature will disable the bit on all other devices,
  // or a Chromebook can enable/disable a phone's BETTER_TOGETHER_HOST bit.
  // Note: Do not confuse with GetDevicesActivityStatus RPC response's
  // |last_update_time|.
  int64_t last_update_time_millis;

  std::map<SoftwareFeature, SoftwareFeatureState> software_features;
  std::vector<BeaconSeed> beacon_seeds;

  // Bluetooth public address, formatted as a hex string with colons and capital
  // letters (example: "01:23:45:67:89:AB"). If the device does not have a
  // synced address, this field is empty.
  std::string bluetooth_public_address;

  RemoteDevice();
  RemoteDevice(
      const std::string& user_email,
      const std::string& instance_id,
      const std::string& name,
      const std::string& pii_free_name,
      const std::string& public_key,
      const std::string& persistent_symmetric_key,
      int64_t last_update_time_millis,
      const std::map<SoftwareFeature, SoftwareFeatureState>& software_features,
      const std::vector<BeaconSeed>& beacon_seeds,
      const std::string& bluetooth_public_address);
  RemoteDevice(const RemoteDevice& other);
  ~RemoteDevice();

  std::string GetDeviceId() const;

  bool operator==(const RemoteDevice& other) const;

  // If at least one of the RemoteDevices has an Instance ID, compare by that;
  // otherwise, compare by public key.
  bool operator<(const RemoteDevice& other) const;
};

typedef std::vector<RemoteDevice> RemoteDeviceList;

}  // namespace ash::multidevice

#endif  // CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_REMOTE_DEVICE_H_
