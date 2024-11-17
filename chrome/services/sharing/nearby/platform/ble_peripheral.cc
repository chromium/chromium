// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_peripheral.h"

namespace nearby::chrome {

BlePeripheral::BlePeripheral(
    bluetooth::mojom::DeviceInfoPtr device_info,
    const std::map<std::string, device::BluetoothUUID>&
        service_id_to_fast_advertisement_service_uuid_map)
    : device_info_(std::move(device_info)),
      service_id_to_fast_advertisement_service_uuid_map_(
          service_id_to_fast_advertisement_service_uuid_map) {}

BlePeripheral::~BlePeripheral() = default;

BlePeripheral::BlePeripheral(BlePeripheral&&) = default;

BlePeripheral& BlePeripheral::operator=(BlePeripheral&&) = default;

std::string BlePeripheral::GetName() const {
  return device_info_->name_for_display;
}

ByteArray BlePeripheral::GetAdvertisementBytes(
    const std::string& service_id) const {
  const auto it_uuid =
      service_id_to_fast_advertisement_service_uuid_map_.find(service_id);
  if (it_uuid == service_id_to_fast_advertisement_service_uuid_map_.end())
    return ByteArray();

  const auto& service_data_map = device_info_->service_data_map;
  const auto it = service_data_map.find(it_uuid->second);
  if (it == service_data_map.end())
    return ByteArray();

  std::string service_data(it->second.begin(), it->second.end());
  return ByteArray(service_data);
}

void BlePeripheral::UpdateDeviceInfo(
    bluetooth::mojom::DeviceInfoPtr device_info) {
  DCHECK_EQ(device_info_->address, device_info->address);
  device_info_ = std::move(device_info);
}

void BlePeripheral::UpdateIdToUuidMap(
    const std::map<std::string, device::BluetoothUUID>&
        service_id_to_fast_advertisement_service_uuid_map) {
  service_id_to_fast_advertisement_service_uuid_map_ =
      service_id_to_fast_advertisement_service_uuid_map;
}

}  // namespace nearby::chrome
