// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_REMOTE_DEVICE_TEST_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_REMOTE_DEVICE_TEST_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "chromeos/ash/components/multidevice/remote_device_ref.h"

namespace ash::multidevice {

// Attributes of the default test remote device.
extern const char kTestRemoteDeviceName[];
extern const char kTestRemoteDevicePublicKey[];
extern const char kTestRemoteDeviceBluetoothPublicAddress[];

class RemoteDeviceRefBuilder {
 public:
  RemoteDeviceRefBuilder();
  ~RemoteDeviceRefBuilder();
  RemoteDeviceRefBuilder& SetUserEmail(const std::string& user_email);
  RemoteDeviceRefBuilder& SetInstanceId(const std::string& instance_id);
  RemoteDeviceRefBuilder& SetName(const std::string& name);
  RemoteDeviceRefBuilder& SetPiiFreeName(const std::string& pii_free_name);
  RemoteDeviceRefBuilder& SetPublicKey(const std::string& public_key);
  RemoteDeviceRefBuilder& SetSupportsMobileHotspot(
      bool supports_mobile_hotspot);
  RemoteDeviceRefBuilder& SetSoftwareFeatureState(
      const SoftwareFeature feature,
      const SoftwareFeatureState new_state);
  RemoteDeviceRefBuilder& SetLastUpdateTimeMillis(
      int64_t last_update_time_millis);
  RemoteDeviceRefBuilder& SetBeaconSeeds(
      const std::vector<BeaconSeed>& beacon_seeds);
  RemoteDeviceRefBuilder& SetBluetoothPublicAddress(
      const std::string& bluetooth_public_address);
  RemoteDeviceRef Build();

 private:
  std::shared_ptr<RemoteDevice> remote_device_;
};

RemoteDevice CreateRemoteDeviceForTest();

RemoteDeviceRef CreateRemoteDeviceRefForTest();

RemoteDeviceList CreateRemoteDeviceListForTest(size_t num_to_create);

RemoteDeviceRefList CreateRemoteDeviceRefListForTest(size_t num_to_create);

RemoteDevice* GetMutableRemoteDevice(const RemoteDeviceRef& remote_device_ref);

bool IsSameDevice(const RemoteDevice& remote_device,
                  RemoteDeviceRef remote_device_ref);

}  // namespace ash::multidevice

#endif  // CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_REMOTE_DEVICE_TEST_UTIL_H_
