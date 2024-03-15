// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_gatt_server.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace {

device::BluetoothGattCharacteristic::Permissions ConvertPermission(
    nearby::api::ble_v2::GattCharacteristic::Permission permission) {
  switch (permission) {
    case nearby::api::ble_v2::GattCharacteristic::Permission::kNone:
      return device::BluetoothGattCharacteristic::Permission::PERMISSION_NONE;
    case nearby::api::ble_v2::GattCharacteristic::Permission::kRead:
      return device::BluetoothGattCharacteristic::Permission::PERMISSION_READ;
    case nearby::api::ble_v2::GattCharacteristic::Permission::kWrite:
      return device::BluetoothGattCharacteristic::Permission::PERMISSION_WRITE;
    case nearby::api::ble_v2::GattCharacteristic::Permission::kLast:
      NOTREACHED_NORETURN();
  }
}

device::BluetoothGattCharacteristic::Properties ConvertProperty(
    nearby::api::ble_v2::GattCharacteristic::Property property) {
  switch (property) {
    case nearby::api::ble_v2::GattCharacteristic::Property::kNone:
      return device::BluetoothGattCharacteristic::Property::PROPERTY_NONE;
    case nearby::api::ble_v2::GattCharacteristic::Property::kRead:
      return device::BluetoothGattCharacteristic::Property::PROPERTY_READ;
    case nearby::api::ble_v2::GattCharacteristic::Property::kWrite:
      return device::BluetoothGattCharacteristic::Property::PROPERTY_WRITE;
    case nearby::api::ble_v2::GattCharacteristic::Property::kIndicate:
      return device::BluetoothGattCharacteristic::Property::PROPERTY_INDICATE;
    case nearby::api::ble_v2::GattCharacteristic::Property::kNotify:
      return device::BluetoothGattCharacteristic::Property::PROPERTY_NOTIFY;
    case nearby::api::ble_v2::GattCharacteristic::Property::kLast:
      NOTREACHED_NORETURN();
  }
}

}  // namespace

namespace nearby::chrome {

BleV2GattServer::BleV2GattServer(
    const mojo::SharedRemote<bluetooth::mojom::Adapter>& adapter)
    : bluetooth_adapter_(std::make_unique<BluetoothAdapter>(adapter)),
      adapter_remote_(adapter) {
  CHECK(adapter_remote_.is_bound());
}

BleV2GattServer::~BleV2GattServer() = default;

BluetoothAdapter& BleV2GattServer::GetBlePeripheral() {
  CHECK(bluetooth_adapter_);
  return *bluetooth_adapter_;
}

std::optional<api::ble_v2::GattCharacteristic>
BleV2GattServer::CreateCharacteristic(
    const Uuid& service_uuid,
    const Uuid& characteristic_uuid,
    api::ble_v2::GattCharacteristic::Permission permission,
    api::ble_v2::GattCharacteristic::Property property) {
  VLOG(1) << "BleV2GattServer::" << __func__;

  // If there isn't a GATT Service that already exists for `service_uuid`,
  // create one in the browser process before creating a characteristic at
  // `characteristic_uuid` tied to the `service_uuid`.
  auto service_it = uuid_to_gatt_service_map_.find(service_uuid);
  if (service_it == uuid_to_gatt_service_map_.end()) {
    mojo::PendingRemote<bluetooth::mojom::GattService>
        gatt_service_pending_remote;
    device::BluetoothUUID bluetooth_service_uuid{std::string(service_uuid)};
    adapter_remote_->CreateLocalGattService(
        /*service_uuid=*/bluetooth_service_uuid,
        /*observer=*/
        gatt_service_observer_.BindNewPipeAndPassRemote(),
        &gatt_service_pending_remote);

    if (!gatt_service_pending_remote) {
      LOG(WARNING) << __func__ << ": Unable to get or create GATT service";
      return std::nullopt;
    }

    auto gatt_service = std::make_unique<GattService>();
    gatt_service->gatt_service_remote.Bind(
        std::move(gatt_service_pending_remote));
    service_it =
        uuid_to_gatt_service_map_.emplace(service_uuid, std::move(gatt_service))
            .first;
  }

  // If a characteristic at `characteristic_uuid` already exists in a GATT
  // service tied to `service_uuid`, return it to callers, and do not
  // attempt to create on in the GATT server. This will only happen if the
  // `GATT service` was not created in the block above because it will only
  // happen if a previous call to `BleV2GattServer::CreateCharacteristic()`
  // created the characteristic.
  auto* gatt_service = service_it->second.get();
  const auto& char_map =
      gatt_service->characteristic_uuid_to_characteristic_map;
  auto char_it = char_map.find(characteristic_uuid);
  if (char_it != char_map.end()) {
    VLOG(1) << __func__ << ": characteristic already exists";
    return char_it->second;
  }

  // Trigger a call in the browser process to create a GATT characteristic in
  // the local device's GATT server. The current implementation of BLE V2
  // in Nearby Connections only supports a single permission or property type
  // for a characteristic, even though the Bluetooth Adapter in the platform
  // layer can support multiple properties using bitwise operations. In order
  // future proof the BLE V2 layer, and keep implementation details of Nearby
  // Connections contained in this class, `BleV2GattServer` converts a single
  // nearby::api::ble_v2::GattCharacteristic Property/Permission into a
  // device::BluetoothGattCharacteristic Permissions/Properties, which
  // only contain a single value.
  CHECK(gatt_service->gatt_service_remote.is_bound());
  bool create_characteristic_success;
  gatt_service->gatt_service_remote->CreateCharacteristic(
      /*characteristic_uuid=*/device::BluetoothUUID(
          characteristic_uuid.Get16BitAsString()),
      /*permissions=*/ConvertPermission(permission),
      /*properties=*/ConvertProperty(property),
      /*out_success=*/&create_characteristic_success);

  if (!create_characteristic_success) {
    LOG(WARNING) << __func__ << ": Unable to create GATT characteristic";
    return std::nullopt;
  }

  // If successful in creating the GATT characteristic, create a corresponding
  // representation of the GATT characteristic to return back to the Nearby
  // Connections library. This will be used to trigger requests to notify or
  // update the GATT characteristic in other methods. The browser process
  // retrieves the corresponding GATT characteristic by `charactertistic_uuid`.
  api::ble_v2::GattCharacteristic gatt_characteristic = {
      characteristic_uuid, service_uuid, permission, property};
  gatt_service->characteristic_uuid_to_characteristic_map.insert_or_assign(
      characteristic_uuid, gatt_characteristic);
  return gatt_characteristic;
}

bool BleV2GattServer::UpdateCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic,
    const nearby::ByteArray& value) {
  // TODO(b/311430390):Implement to call on the Mojo remote to update the value
  // of the GATT Characteristic, and returns the success/failure of the
  // operation.
  NOTIMPLEMENTED();
  return false;
}

absl::Status BleV2GattServer::NotifyCharacteristicChanged(
    const api::ble_v2::GattCharacteristic& characteristic,
    bool confirm,
    const nearby::ByteArray& new_value) {
  // TODO(b/311430390): Implement to call on the Mojo remote to update the value
  // of the GATT Characteristic, and notify remote devices, and returns the
  // resulting Status of the operation.
  NOTIMPLEMENTED();
  return absl::Status();
}

void BleV2GattServer::Stop() {
  // TODO(b/311430390): Implement to call on the Mojo remote to stop the GATT
  // server.
  NOTIMPLEMENTED();
}

BleV2GattServer::GattService::GattService() = default;
BleV2GattServer::GattService::~GattService() = default;

}  // namespace nearby::chrome
