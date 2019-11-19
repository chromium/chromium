// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MULTIDEVICE_REMOTE_DEVICE_H_
#define CHROMEOS_COMPONENTS_MULTIDEVICE_REMOTE_DEVICE_H_

#include <map>
#include <string>
#include <vector>

#include "chromeos/components/multidevice/beacon_seed.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/components/multidevice/software_feature_state.h"

namespace chromeos {

namespace multidevice {

struct RemoteDevice {
  // Generates the device ID for a device given its public key.
  static std::string GenerateDeviceId(const std::string& public_key);

  // Derives the public key that was used to generate the given device ID;
  // returns empty string if |device_id| is not a valid device ID.
  static std::string DerivePublicKey(const std::string& device_id);

  std::string user_id;

  // The Instance ID is the primary identifier for devices using CryptAuth v2,
  // but the Instance ID is not present in CryptAuth v1. This string is empty
  // for devices not using CryptAuth v2 Enrollment and v2 DeviceSync.
  // TODO(https://crbug.com/1019206): Remove comments when v1 DeviceSync is
  // deprecated.
  std::string instance_id;

  std::string name;
  std::string pii_free_name;
  std::string public_key;
  std::string persistent_symmetric_key;
  int64_t last_update_time_millis;
  std::map<SoftwareFeature, SoftwareFeatureState> software_features;
  std::vector<BeaconSeed> beacon_seeds;

  RemoteDevice();
  RemoteDevice(
      const std::string& user_id,
      const std::string& instance_id,
      const std::string& name,
      const std::string& pii_free_name,
      const std::string& public_key,
      const std::string& persistent_symmetric_key,
      int64_t last_update_time_millis,
      const std::map<SoftwareFeature, SoftwareFeatureState>& software_features,
      const std::vector<BeaconSeed>& beacon_seeds);
  RemoteDevice(const RemoteDevice& other);
  ~RemoteDevice();

  std::string GetDeviceId() const;

  bool operator==(const RemoteDevice& other) const;

  // Compares devices via their public keys. Note that this function is
  // necessary in order to use |RemoteDevice| as a key of a std::map.
  bool operator<(const RemoteDevice& other) const;
};

typedef std::vector<RemoteDevice> RemoteDeviceList;

}  // namespace multidevice

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MULTIDEVICE_REMOTE_DEVICE_H_
