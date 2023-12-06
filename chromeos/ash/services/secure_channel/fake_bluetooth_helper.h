// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_BLUETOOTH_HELPER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_BLUETOOTH_HELPER_H_

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/secure_channel/bluetooth_helper.h"
#include "chromeos/ash/services/secure_channel/data_with_timestamp.h"

namespace ash::secure_channel {

// Test BluetoothHelper implementation.
class FakeBluetoothHelper : public BluetoothHelper {
 public:
  FakeBluetoothHelper();

  FakeBluetoothHelper(const FakeBluetoothHelper&) = delete;
  FakeBluetoothHelper& operator=(const FakeBluetoothHelper&) = delete;

  ~FakeBluetoothHelper() override;

  // Sets the data to be returned by a GenerateForegroundAdvertisement() call.
  void SetAdvertisement(const DeviceIdPair& device_id_pair,
                        const DataWithTimestamp& service_data);

  void RemoveAdvertisement(const DeviceIdPair& device_id_pair);

  // Sets the identified device to be returned by a IdentifyRemoteDevice() call.
  void SetIdentifiedDevice(const std::string& service_data,
                           multidevice::RemoteDeviceRef identified_device,
                           bool is_background_advertisement);

  // Sets the data to be returned by a GetBluetoothPublicAddress() call.
  void SetBluetoothPublicAddress(const std::string& device_id,
                                 const std::string& bluetooth_public_address);

 private:
  // BluetoothHelper:
  std::unique_ptr<DataWithTimestamp> GenerateForegroundAdvertisement(
      const DeviceIdPair& device_id_pair) override;
  std::optional<DeviceWithBackgroundBool> PerformIdentifyRemoteDevice(
      const std::string& service_data,
      const DeviceIdPairSet& device_id_pair_set) override;
  std::string GetBluetoothPublicAddress(const std::string& device_id) override;
  std::string ExpectedServiceDataToString(
      const DeviceIdPairSet& device_id_pair_set) override;

  std::unordered_map<DeviceIdPair, DataWithTimestamp, DeviceIdPairHash>
      device_id_pair_to_service_data_map_;

  std::unordered_map<std::string, DeviceWithBackgroundBool>
      service_data_to_device_with_background_bool_map_;

  std::unordered_map<std::string, std::string>
      device_id_to_bluetooth_public_address_map_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_BLUETOOTH_HELPER_H_
