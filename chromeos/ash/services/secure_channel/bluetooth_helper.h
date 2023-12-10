// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLUETOOTH_HELPER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLUETOOTH_HELPER_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/secure_channel/data_with_timestamp.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"

namespace ash::secure_channel {

// Provides the ability to generate BLE advertisement service data and, given
// service data that has been received in a BLE discovery session, identify the
// device which sent the advertisement.
//
// Also provides functionality to retrieve the Bluetooth public address for a
// device for use in Bluetooth Classic connections.
class BluetoothHelper {
 public:
  BluetoothHelper(const BluetoothHelper&) = delete;
  BluetoothHelper& operator=(const BluetoothHelper&) = delete;

  virtual ~BluetoothHelper();

  // Generates service data to be used in a foreground BLE advertisement from
  // the device with ID |local_device_id| to the device with ID
  // |remote_device_id|. If no service data can be generated, null is returned.
  virtual std::unique_ptr<DataWithTimestamp> GenerateForegroundAdvertisement(
      const DeviceIdPair& device_id_pair) = 0;

  // Remote device paired with a boolean of whether the device was identified
  // via the background advertisement scheme.
  using DeviceWithBackgroundBool =
      std::pair<multidevice::RemoteDeviceRef, bool>;

  // Identifies the device that produced a BLE advertisement with service data
  // |service_data|. If no device can be identified, std::nullopt is returned.
  std::optional<DeviceWithBackgroundBool> IdentifyRemoteDevice(
      const std::string& service_data,
      const DeviceIdPairSet& device_id_pair_set);

  // Note: An empty string is returned if there is no known public address.
  virtual std::string GetBluetoothPublicAddress(
      const std::string& device_id) = 0;

  // Prints a string containing the expected service data for the provided
  // device IDs.
  virtual std::string ExpectedServiceDataToString(
      const DeviceIdPairSet& device_id_pair_set) = 0;

 protected:
  BluetoothHelper();

  virtual std::optional<DeviceWithBackgroundBool> PerformIdentifyRemoteDevice(
      const std::string& service_data,
      const DeviceIdPairSet& device_id_pair_set) = 0;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLUETOOTH_HELPER_H_
