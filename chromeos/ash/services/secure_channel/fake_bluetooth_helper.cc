// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/fake_bluetooth_helper.h"

#include "base/containers/contains.h"

namespace ash::secure_channel {

FakeBluetoothHelper::FakeBluetoothHelper() = default;

FakeBluetoothHelper::~FakeBluetoothHelper() = default;

void FakeBluetoothHelper::SetAdvertisement(
    const DeviceIdPair& device_id_pair,
    const DataWithTimestamp& service_data) {
  device_id_pair_to_service_data_map_.insert({device_id_pair, service_data});
}

void FakeBluetoothHelper::RemoveAdvertisement(
    const DeviceIdPair& device_id_pair) {
  device_id_pair_to_service_data_map_.erase(device_id_pair);
}

void FakeBluetoothHelper::SetIdentifiedDevice(
    const std::string& service_data,
    multidevice::RemoteDeviceRef identified_device,
    bool is_background_advertisement) {
  service_data_to_device_with_background_bool_map_.insert(
      {service_data, DeviceWithBackgroundBool(identified_device,
                                              is_background_advertisement)});
}

void FakeBluetoothHelper::SetBluetoothPublicAddress(
    const std::string& device_id,
    const std::string& bluetooth_public_address) {
  device_id_to_bluetooth_public_address_map_[device_id] =
      bluetooth_public_address;
}

std::unique_ptr<DataWithTimestamp>
FakeBluetoothHelper::GenerateForegroundAdvertisement(
    const DeviceIdPair& device_id_pair) {
  if (!base::Contains(device_id_pair_to_service_data_map_, device_id_pair))
    return nullptr;

  return std::make_unique<DataWithTimestamp>(
      device_id_pair_to_service_data_map_.at(device_id_pair));
}

std::optional<BluetoothHelper::DeviceWithBackgroundBool>
FakeBluetoothHelper::PerformIdentifyRemoteDevice(
    const std::string& service_data,
    const DeviceIdPairSet& device_id_pair_set) {
  if (!base::Contains(service_data_to_device_with_background_bool_map_,
                      service_data)) {
    return std::nullopt;
  }

  return service_data_to_device_with_background_bool_map_.at(service_data);
}

std::string FakeBluetoothHelper::GetBluetoothPublicAddress(
    const std::string& device_id) {
  return device_id_to_bluetooth_public_address_map_[device_id];
}

std::string FakeBluetoothHelper::ExpectedServiceDataToString(
    const DeviceIdPairSet& device_id_pair_set) {
  // Stub implementation.
  return std::string();
}

}  // namespace ash::secure_channel
