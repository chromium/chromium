// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform_v2/ble_medium.h"

#include "chrome/services/sharing/nearby/platform_v2/bluetooth_device.h"

namespace location {
namespace nearby {
namespace chrome {

BleMedium::BleMedium(bluetooth::mojom::Adapter* adapter) : adapter_(adapter) {}

BleMedium::~BleMedium() {
  for (auto& it : registered_advertisements_map_) {
    // Note: this call is blocking.
    it.second->Unregister();
  }
}

bool BleMedium::StartAdvertising(const std::string& service_id,
                                 const ByteArray& advertisement) {
  StopAdvertising(service_id);

  auto service_uuid = device::BluetoothUUID(service_id);
  mojo::PendingRemote<bluetooth::mojom::Advertisement> pending_advertisement;
  bool success = adapter_->RegisterAdvertisement(
      service_uuid,
      std::vector<uint8_t>(advertisement.data(),
                           advertisement.data() + advertisement.size()),
      &pending_advertisement);

  if (!success || !pending_advertisement.is_valid())
    return false;

  auto& remote_advertisement =
      registered_advertisements_map_
          .emplace(service_uuid, std::move(pending_advertisement))
          .first->second;
  remote_advertisement.set_disconnect_handler(base::BindOnce(
      &BleMedium::AdvertisementReleased, base::Unretained(this), service_uuid));

  return true;
}

bool BleMedium::StopAdvertising(const std::string& service_id) {
  auto it =
      registered_advertisements_map_.find(device::BluetoothUUID(service_id));
  if (it == registered_advertisements_map_.end())
    return true;

  bool success = it->second->Unregister();
  registered_advertisements_map_.erase(it);

  return success;
}

bool BleMedium::StartScanning(const std::string& service_id,
                              api::BleMedium::DiscoveredPeripheralCallback
                                  discovered_peripheral_callback) {
  auto service_uuid = device::BluetoothUUID(service_id);
  if (IsScanning() &&
      base::Contains(discovered_peripheral_callbacks_map_, service_uuid)) {
    return true;
  }

  // We only need to start discovery if no other discovery request is active.
  if (discovered_peripheral_callbacks_map_.empty()) {
    discovered_ble_peripherals_map_.clear();

    bool success =
        adapter_->AddObserver(adapter_observer_.BindNewPipeAndPassRemote());
    if (!success) {
      adapter_observer_.reset();
      return false;
    }

    mojo::PendingRemote<bluetooth::mojom::DiscoverySession> discovery_session;
    success = adapter_->StartDiscoverySession(&discovery_session);

    if (!success || !discovery_session.is_valid()) {
      adapter_observer_.reset();
      return false;
    }

    discovery_session_.Bind(std::move(discovery_session));
    discovery_session_.set_disconnect_handler(
        base::BindOnce(&BleMedium::DiscoveringChanged, base::Unretained(this),
                       /*discovering=*/false));
  }

  // A different DiscoveredPeripheralCallback is being passed on each call, so
  // each must be captured and associated with its |service_id|.
  discovered_peripheral_callbacks_map_.insert(
      {service_uuid, discovered_peripheral_callback});
  return true;
}

bool BleMedium::StopScanning(const std::string& service_id) {
  discovered_peripheral_callbacks_map_.erase(device::BluetoothUUID(service_id));
  if (!discovered_peripheral_callbacks_map_.empty())
    return true;

  bool stop_discovery_success = true;
  if (discovery_session_) {
    bool message_success = discovery_session_->Stop(&stop_discovery_success);
    stop_discovery_success = stop_discovery_success && message_success;
  }

  adapter_observer_.reset();
  discovery_session_.reset();

  return stop_discovery_success;
}

bool BleMedium::StartAcceptingConnections(
    const std::string& service_id,
    api::BleMedium::AcceptedConnectionCallback accepted_connection_callback) {
  // Do not actually start a GATT server, because BLE connections are not yet
  // supported in Chrome Nearby. However, return true in order to allow
  // BLE advertising to continue.

  // TODO(hansberry): Verify if this is still required in NCv2.
  return true;
}

bool BleMedium::StopAcceptingConnections(const std::string& service_id) {
  // Do nothing. BLE connections are not yet supported in Chrome Nearby.
  return false;
}

std::unique_ptr<api::BleSocket> BleMedium::Connect(
    api::BlePeripheral& ble_peripheral,
    const std::string& service_id) {
  // Do nothing. BLE connections are not yet supported in Chrome Nearby.
  return nullptr;
}

void BleMedium::PresentChanged(bool present) {
  // TODO(hansberry): It is unclear to me how the API implementation can signal
  // to Core that |present| has become unexpectedly false. Need to ask
  // Nearby team.
  if (!present)
    StopScanning();
}

void BleMedium::PoweredChanged(bool powered) {
  // TODO(hansberry): It is unclear to me how the API implementation can signal
  // to Core that |powered| has become unexpectedly false. Need to ask
  // Nearby team.
  if (!powered)
    StopScanning();
}

void BleMedium::DiscoverableChanged(bool discoverable) {
  // Do nothing. BleMedium is not responsible for managing
  // discoverable state.
}

void BleMedium::DiscoveringChanged(bool discovering) {
  // TODO(hansberry): It is unclear to me how the API implementation can signal
  // to Core that |discovering| has become unexpectedly false. Need to ask
  // Nearby team.
  if (!discovering)
    StopScanning();
}

void BleMedium::DeviceAdded(bluetooth::mojom::DeviceInfoPtr device) {
  if (!IsScanning())
    return;

  // Best-effort attempt to filter out BT Classic devices. Dual-mode (BT
  // Classic and BLE) devices which the system has paired and/or connected to
  // may also expose service data, but all BLE advertisements that we are
  // interested in are captured in an element of |service_data_map|. See
  // BluetoothClassicMedium for separate discovery of BT Classic devices.
  if (device->service_data_map.empty())
    return;

  const std::string& address = device->address;
  auto* ble_peripheral = GetDiscoveredBlePeripheral(address);
  if (ble_peripheral)
    ble_peripheral->UpdateDeviceInfo(std::move(device));
  else
    discovered_ble_peripherals_map_.emplace(address, std::move(device));

  // Invoking one of the callbacks in |discovered_peripheral_callbacks_map_| may
  // lead to invalidating one or all elements of
  // |discovered_peripheral_callbacks_map_|, e.g., triggering StopScanning()
  // while looping through it. Callbacks are copied to ensure they are not
  // modified as we loop through them.
  auto callbacks_map_copy = discovered_peripheral_callbacks_map_;
  for (auto& it : callbacks_map_copy) {
    // Must fetch |ble_peripheral| again because it may have been invalidated by
    // a prior callback in this loop.
    ble_peripheral = GetDiscoveredBlePeripheral(address);
    if (!ble_peripheral)
      break;

    const auto& service_id = it.first.value();
    if (ble_peripheral->GetAdvertisementBytes(service_id).Empty())
      continue;

    it.second.peripheral_discovered_cb(*ble_peripheral, service_id);
  }
}

void BleMedium::DeviceChanged(bluetooth::mojom::DeviceInfoPtr device) {
  DeviceAdded(std::move(device));
}

void BleMedium::DeviceRemoved(bluetooth::mojom::DeviceInfoPtr device) {
  if (!IsScanning())
    return;

  const std::string& address = device->address;
  if (!GetDiscoveredBlePeripheral(address))
    return;

  // Invoking one of the callbacks in |discovered_peripheral_callbacks_map_| may
  // lead to invalidating one or all elements of
  // |discovered_peripheral_callbacks_map_|, e.g., triggering StopScanning()
  // while looping through it. Callbacks are copied to ensure they are not
  // modified as we loop through them.
  auto callbacks_map_copy = discovered_peripheral_callbacks_map_;
  for (auto& it : callbacks_map_copy) {
    // Must fetch |ble_peripheral| again because it may have been invalidated by
    // a prior callback in this loop.
    auto* ble_peripheral = GetDiscoveredBlePeripheral(address);
    if (!ble_peripheral)
      break;

    it.second.peripheral_lost_cb(*ble_peripheral,
                                 /*service_id=*/it.first.value());
  }

  discovered_ble_peripherals_map_.erase(address);
}

void BleMedium::AdvertisementReleased(
    const device::BluetoothUUID& service_uuid) {
  registered_advertisements_map_.erase(service_uuid);
}

bool BleMedium::IsScanning() {
  return adapter_observer_.is_bound() && discovery_session_.is_bound() &&
         !discovered_peripheral_callbacks_map_.empty();
}

void BleMedium::StopScanning() {
  // We cannot simply iterate over |discovered_peripheral_callbacks_map_|
  // because StopScanning() will erase the provided element.
  while (!discovered_peripheral_callbacks_map_.empty()) {
    StopScanning(/*service_id=*/discovered_peripheral_callbacks_map_.begin()
                     ->first.value());
  }
}

chrome::BlePeripheral* BleMedium::GetDiscoveredBlePeripheral(
    const std::string& address) {
  auto it = discovered_ble_peripherals_map_.find(address);
  return it == discovered_ble_peripherals_map_.end() ? nullptr : &it->second;
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
