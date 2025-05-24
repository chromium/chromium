// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/services/sharing/nearby/platform/ble_medium.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/services/sharing/nearby/platform/bluetooth_device.h"

namespace nearby::chrome {

namespace {
// Client name for logging in BLE scanning.
constexpr char kScanClientName[] = "Nearby Connections";

void LogStartAdvertisingResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.Bluetooth.LEMedium.StartAdvertising.Result", success);
}

void LogStopAdvertisingResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.Bluetooth.LEMedium.StopAdvertising.Result", success);
}

void LogStartScanningResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.Bluetooth.LEMedium.StartScanning.Result", success);
}

void LogStopScanningResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.Bluetooth.LEMedium.StopScanning.Result", success);
}
}  // namespace

BleMedium::BleMedium(
    const mojo::SharedRemote<bluetooth::mojom::Adapter>& adapter)
    : adapter_(adapter) {
  DCHECK(adapter_.is_bound());
}

BleMedium::~BleMedium() {
  for (auto& it : registered_advertisements_map_) {
    // Note: this call is blocking.
    it.second->Unregister();
  }
}

bool BleMedium::StartAdvertising(
    const std::string& service_id,
    const ByteArray& advertisement,
    const std::string& fast_advertisement_service_uuid) {
  // Chrome Nearby BLE cannot support regular advertisements; only Fast
  // Advertisements. Ensure |fast_advertisement_service_uuid| is provided.
  DCHECK(!fast_advertisement_service_uuid.empty());

  StopAdvertising(service_id);

  auto service_uuid = device::BluetoothUUID(fast_advertisement_service_uuid);

  mojo::PendingRemote<bluetooth::mojom::Advertisement> pending_advertisement;
  bool success = adapter_->RegisterAdvertisement(
      service_uuid,
      std::vector<uint8_t>(advertisement.data(),
                           advertisement.data() + advertisement.size()),
      /*use_scan_data=*/true, /*connectable=*/false, &pending_advertisement);

  if (!success || !pending_advertisement.is_valid()) {
    LogStartAdvertisingResult(false);
    return false;
  }

  registered_service_id_to_fast_advertisement_service_uuid_map_.emplace(
      service_id, service_uuid);

  auto& remote_advertisement =
      registered_advertisements_map_
          .emplace(service_uuid, std::move(pending_advertisement))
          .first->second;
  remote_advertisement.set_disconnect_handler(base::BindOnce(
      &BleMedium::AdvertisementReleased, base::Unretained(this), service_uuid));

  LogStartAdvertisingResult(true);
  return true;
}

bool BleMedium::StopAdvertising(const std::string& service_id) {
  auto uuid_it =
      registered_service_id_to_fast_advertisement_service_uuid_map_.find(
          service_id);
  if (uuid_it ==
      registered_service_id_to_fast_advertisement_service_uuid_map_.end()) {
    LogStopAdvertisingResult(true);
    return true;
  }

  auto advertisement_it = registered_advertisements_map_.find(uuid_it->second);
  registered_service_id_to_fast_advertisement_service_uuid_map_.erase(uuid_it);

  if (advertisement_it == registered_advertisements_map_.end()) {
    LogStopAdvertisingResult(true);
    return true;
  }

  bool success = advertisement_it->second->Unregister();
  registered_advertisements_map_.erase(advertisement_it);

  LogStopAdvertisingResult(success);
  return success;
}

bool BleMedium::StartScanning(
    const std::string& service_id,
    const std::string& fast_advertisement_service_uuid,
    api::BleMedium::DiscoveredPeripheralCallback
        discovered_peripheral_callback) {
  auto service_uuid = device::BluetoothUUID(fast_advertisement_service_uuid);

  // The ID-to-UUID map should always be in sync with the callbacks map, and we
  // assume that the ID-UUID mapping is one-to-one.
  DCHECK_EQ(base::Contains(discovered_peripheral_callbacks_map_, service_uuid),
            base::Contains(
                discovery_service_id_to_fast_advertisement_service_uuid_map_,
                service_id));

  if (IsScanning() &&
      base::Contains(discovered_peripheral_callbacks_map_, service_uuid)) {
    LogStartScanningResult(true);
    return true;
  }

  // The ID-to-UUID map should always be in sync with the callbacks map.
  DCHECK_EQ(
      discovered_peripheral_callbacks_map_.empty(),
      discovery_service_id_to_fast_advertisement_service_uuid_map_.empty());

  // We only need to start discovery if no other discovery request is active.
  if (discovered_peripheral_callbacks_map_.empty()) {
    discovered_ble_peripherals_map_.clear();

    bool success =
        adapter_->AddObserver(adapter_observer_.BindNewPipeAndPassRemote());
    if (!success) {
      adapter_observer_.reset();
      LogStartScanningResult(false);
      return false;
    }

    mojo::PendingRemote<bluetooth::mojom::DiscoverySession> discovery_session;
    success =
        adapter_->StartDiscoverySession(kScanClientName, &discovery_session);

    if (!success || !discovery_session.is_valid()) {
      adapter_observer_.reset();
      LogStartScanningResult(false);
      return false;
    }

    discovery_session_.Bind(std::move(discovery_session));
    discovery_session_.set_disconnect_handler(
        base::BindOnce(&BleMedium::DiscoveringChanged, base::Unretained(this),
                       /*discovering=*/false));
  }

  // A different DiscoveredPeripheralCallback is being passed on each call, so
  // each must be captured and associated with its service UUID.
  discovered_peripheral_callbacks_map_.insert(
      {service_uuid, std::move(discovered_peripheral_callback)});

  discovery_service_id_to_fast_advertisement_service_uuid_map_.insert(
      {service_id, service_uuid});
  for (auto& uuid_peripheral_pair : discovered_ble_peripherals_map_) {
    uuid_peripheral_pair.second.UpdateIdToUuidMap(
        discovery_service_id_to_fast_advertisement_service_uuid_map_);
  }

  LogStartScanningResult(true);
  return true;
}

bool BleMedium::StopScanning(const std::string& service_id) {
  const auto it =
      discovery_service_id_to_fast_advertisement_service_uuid_map_.find(
          service_id);
  if (it !=
      discovery_service_id_to_fast_advertisement_service_uuid_map_.end()) {
    DCHECK(base::Contains(discovered_peripheral_callbacks_map_, it->second));
    discovered_peripheral_callbacks_map_.erase(it->second);
    discovery_service_id_to_fast_advertisement_service_uuid_map_.erase(it);
    for (auto& uuid_peripheral_pair : discovered_ble_peripherals_map_) {
      uuid_peripheral_pair.second.UpdateIdToUuidMap(
          discovery_service_id_to_fast_advertisement_service_uuid_map_);
    }
  }

  // The ID-to-UUID map should always be in sync with the callbacks map.
  DCHECK_EQ(
      discovered_peripheral_callbacks_map_.empty(),
      discovery_service_id_to_fast_advertisement_service_uuid_map_.empty());

  if (!discovered_peripheral_callbacks_map_.empty()) {
    LogStopScanningResult(true);
    return true;
  }

  bool stop_discovery_success = true;
  if (discovery_session_) {
    bool message_success = discovery_session_->Stop(&stop_discovery_success);
    stop_discovery_success = stop_discovery_success && message_success;
  }

  adapter_observer_.reset();
  discovery_session_.reset();

  LogStopScanningResult(stop_discovery_success);
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
    const std::string& service_id,
    CancellationFlag* cancellation_flag) {
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

  // Add a new or update the existing discovered peripheral. Note: Because
  // BlePeripherals are passed by reference to NearbyConnections, if a
  // BlePeripheral already exists with the given address, the reference should
  // not be invalidated, the update functions should be called instead.
  const std::string address = device->address;
  auto* existing_ble_peripheral = GetDiscoveredBlePeripheral(address);
  if (existing_ble_peripheral) {
    existing_ble_peripheral->UpdateDeviceInfo(std::move(device));
  } else {
    discovered_ble_peripherals_map_.emplace(
        address,
        chrome::BlePeripheral(
            std::move(device),
            discovery_service_id_to_fast_advertisement_service_uuid_map_));
  }

  // Copy the ID-to-UUID map to ensure that elements are not invalidated while
  // iterating--for example, if StopScanning() is triggered after invoking the
  // callback in the body of the loop.
  auto id_uuid_map_copy =
      discovery_service_id_to_fast_advertisement_service_uuid_map_;
  for (const auto& id_uuid_pair : id_uuid_map_copy) {
    // A callback should always be found unless an element was removed while we
    // were iterating through the IDs.
    const auto it =
        discovered_peripheral_callbacks_map_.find(id_uuid_pair.second);
    if (it == discovered_peripheral_callbacks_map_.end())
      continue;

    // Fetch the BlePeripheral with the same `address` again because
    // previously fetched pointers may have been invalidated while iterating
    // through the IDs.
    auto* ble_peripheral = GetDiscoveredBlePeripheral(address);
    if (!ble_peripheral)
      continue;

    // Do not perform any filtering here, for example, by checking if the
    // peripheral has non-empty advertisement bytes. Unconditionally inform all
    // callbacks of the discovered device, and rely on the Nearby Connections
    // library to perform the filtering.
    it->second.peripheral_discovered_cb(*ble_peripheral, id_uuid_pair.first,
                                        /*fast_advertisement=*/true);
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

  // Copy the ID-to-UUID map to ensure that elements are not invalidated while
  // iterating--for example, if StopScanning() is triggered after invoking the
  // callback in the body of the loop.
  auto id_uuid_map_copy =
      discovery_service_id_to_fast_advertisement_service_uuid_map_;
  for (const auto& id_uuid_pair : id_uuid_map_copy) {
    // A callback should always be found unless an element was removed while we
    // were iterating through the IDs.
    const auto it =
        discovered_peripheral_callbacks_map_.find(id_uuid_pair.second);
    if (it == discovered_peripheral_callbacks_map_.end())
      continue;

    // Fetch |ble_peripheral| again because it might have since been invalidated
    // while we were iterating through IDs.
    auto* ble_peripheral = GetDiscoveredBlePeripheral(address);
    if (!ble_peripheral)
      continue;

    it->second.peripheral_lost_cb(*ble_peripheral, id_uuid_pair.first);
  }
}

void BleMedium::AdvertisementReleased(
    const device::BluetoothUUID& service_uuid) {
  registered_advertisements_map_.erase(service_uuid);
}

bool BleMedium::IsScanning() {
  DCHECK_EQ(
      discovered_peripheral_callbacks_map_.empty(),
      discovery_service_id_to_fast_advertisement_service_uuid_map_.empty());
  return adapter_observer_.is_bound() && discovery_session_.is_bound() &&
         !discovered_peripheral_callbacks_map_.empty();
}

void BleMedium::StopScanning() {
  // We cannot simply iterate over
  // |discovery_service_id_to_fast_advertisement_service_uuid_map_| because
  // StopScanning() will erase the provided element.
  while (
      !discovery_service_id_to_fast_advertisement_service_uuid_map_.empty()) {
    StopScanning(
        /*service_id=*/
        discovery_service_id_to_fast_advertisement_service_uuid_map_.begin()
            ->first);
  }
  DCHECK(discovered_peripheral_callbacks_map_.empty());
}

chrome::BlePeripheral* BleMedium::GetDiscoveredBlePeripheral(
    const std::string& address) {
  auto it = discovered_ble_peripherals_map_.find(address);
  return it == discovered_ble_peripherals_map_.end() ? nullptr : &it->second;
}

}  // namespace nearby::chrome
