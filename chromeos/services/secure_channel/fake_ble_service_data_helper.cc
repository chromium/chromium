// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_ble_service_data_helper.h"

#include "base/stl_util.h"

namespace chromeos {

namespace secure_channel {

FakeBleServiceDataHelper::FakeBleServiceDataHelper() = default;

FakeBleServiceDataHelper::~FakeBleServiceDataHelper() = default;

void FakeBleServiceDataHelper::SetAdvertisement(
    const DeviceIdPair& device_id_pair,
    const DataWithTimestamp& service_data) {
  device_id_pair_to_service_data_map_.insert({device_id_pair, service_data});
}

void FakeBleServiceDataHelper::RemoveAdvertisement(
    const DeviceIdPair& device_id_pair) {
  device_id_pair_to_service_data_map_.erase(device_id_pair);
}

void FakeBleServiceDataHelper::SetIdentifiedDevice(
    const std::string& service_data,
    multidevice::RemoteDeviceRef identified_device,
    bool is_background_advertisement) {
  service_data_to_device_with_background_bool_map_.insert(
      {service_data, DeviceWithBackgroundBool(identified_device,
                                              is_background_advertisement)});
}

std::unique_ptr<DataWithTimestamp>
FakeBleServiceDataHelper::GenerateForegroundAdvertisement(
    const DeviceIdPair& device_id_pair) {
  if (!base::Contains(device_id_pair_to_service_data_map_, device_id_pair))
    return nullptr;

  return std::make_unique<DataWithTimestamp>(
      device_id_pair_to_service_data_map_.at(device_id_pair));
}

base::Optional<BleServiceDataHelper::DeviceWithBackgroundBool>
FakeBleServiceDataHelper::PerformIdentifyRemoteDevice(
    const std::string& service_data,
    const DeviceIdPairSet& device_id_pair_set) {
  if (!base::Contains(service_data_to_device_with_background_bool_map_,
                      service_data)) {
    return base::nullopt;
  }

  return service_data_to_device_with_background_bool_map_.at(service_data);
}

}  // namespace secure_channel

}  // namespace chromeos
