// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/services/sharing/nearby/platform/ble_v2_medium.h"

#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "chrome/services/sharing/nearby/platform/ble_v2_peripheral.h"
#include "third_party/nearby/src/internal/platform/byte_array.h"
#include "third_party/nearby/src/internal/platform/implementation/ble_v2.h"

namespace nearby::chrome {

namespace {
// Max times trying to generate unique scan session id.
static constexpr int kGenerateSessionIdRetryLimit = 3;
// Indicating failed to generate unique scan session id.
static constexpr uint64_t kFailedGenerateSessionId = 0;

// Client name for logging in BLE scanning.
static constexpr char kScanClientName[] = "NearbyBleV2";
}  // namespace

BleV2Medium::BleV2Medium() {
  LOG(WARNING) << "BleV2Medium default constructor not implemented yet.";
}

BleV2Medium::BleV2Medium(
    const mojo::SharedRemote<bluetooth::mojom::Adapter>& adapter)
    : adapter_(adapter) {
  DCHECK(adapter_.is_bound());
}

BleV2Medium::~BleV2Medium() {
  LOG(WARNING) << "BleV2Medium destructor not implemented yet.";
}

bool BleV2Medium::StartAdvertising(
    const api::ble_v2::BleAdvertisementData& advertising_data,
    api::ble_v2::AdvertiseParameters advertise_set_parameters) {
  NOTIMPLEMENTED();
  return false;
}

std::unique_ptr<BleV2Medium::AdvertisingSession> BleV2Medium::StartAdvertising(
    const api::ble_v2::BleAdvertisementData& advertising_data,
    api::ble_v2::AdvertiseParameters advertise_set_parameters,
    BleV2Medium::AdvertisingCallback callback) {
  NOTIMPLEMENTED();
  return nullptr;
}

bool BleV2Medium::StopAdvertising() {
  NOTIMPLEMENTED();
  return false;
}

bool BleV2Medium::StartScanning(const Uuid& service_uuid,
                                api::ble_v2::TxPowerLevel tx_power_level,
                                BleV2Medium::ScanCallback callback) {
  NOTIMPLEMENTED();
  return false;
}

bool BleV2Medium::StopScanning() {
  NOTIMPLEMENTED();
  return false;
}

// Fake impl to return hard coded advertisement.
std::unique_ptr<BleV2Medium::ScanningSession> BleV2Medium::StartScanning(
    const Uuid& service_uuid,
    api::ble_v2::TxPowerLevel tx_power_level,
    BleV2Medium::ScanningCallback callback) {
  if (!IsScanning()) {
    discovered_ble_peripherals_map_.clear();
    service_uuid_to_session_ids_map_.clear();
    session_id_to_scanning_callback_map_.clear();

    bool success =
        adapter_->AddObserver(adapter_observer_.BindNewPipeAndPassRemote());
    if (!success) {
      adapter_observer_.reset();
      return nullptr;
    }

    mojo::PendingRemote<bluetooth::mojom::DiscoverySession> discovery_session;
    success =
        adapter_->StartDiscoverySession(kScanClientName, &discovery_session);

    if (!success || !discovery_session.is_valid()) {
      adapter_observer_.reset();
      return nullptr;
    }

    discovery_session_.Bind(std::move(discovery_session));
    discovery_session_.set_disconnect_handler(
        base::BindOnce(&BleV2Medium::DiscoveringChanged, base::Unretained(this),
                       /*discovering=*/false));
  }
  if (callback.start_scanning_result) {
    callback.start_scanning_result(absl::OkStatus());
  }

  // A "service" refers to high-level libraries like Connections, Presence,
  // Fast Pair, etc. Each service has a unique `service_uuid`, and each client
  // application that consumes that service will be making requests to
  // `StartScanning()` with the same `service_uuid`. In order to disambiguate
  // multiple clients using the same `service_uuid`, we create a `session_id`
  // here for each scan request.
  uint64_t session_id = GenerateUniqueSessionId();

  device::BluetoothUUID bluetooth_service_uuid{std::string(service_uuid)};

  // Save session id, service id and callback for this scan session.
  session_id_to_scanning_callback_map_.insert(
      {session_id, std::move(callback)});
  auto iter = service_uuid_to_session_ids_map_.find(bluetooth_service_uuid);
  if (iter == service_uuid_to_session_ids_map_.end()) {
    service_uuid_to_session_ids_map_.insert(
        {bluetooth_service_uuid, {session_id}});
  } else {
    iter->second.insert(session_id);
  }

  // Generate and return ScanningSession.
  return std::make_unique<BleV2Medium::ScanningSession>(
      BleV2Medium::ScanningSession{
          .stop_scanning =
              [this, session_id, bluetooth_service_uuid]() {
                size_t num_erased_from_callback_map =
                    session_id_to_scanning_callback_map_.erase(session_id);

                size_t num_erased_from_service_and_session_map = 0u;
                auto iter = service_uuid_to_session_ids_map_.find(
                    bluetooth_service_uuid);
                if (iter != service_uuid_to_session_ids_map_.end()) {
                  num_erased_from_service_and_session_map =
                      iter->second.erase(session_id);
                }
                if (num_erased_from_callback_map != 1u ||
                    num_erased_from_service_and_session_map != 1u) {
                  return absl::NotFoundError(
                      "Can't find the provided internal session");
                }

                session_id_to_scanning_callback_map_.erase(session_id);
                service_uuid_to_session_ids_map_[bluetooth_service_uuid].erase(
                    session_id);
                if (service_uuid_to_session_ids_map_[bluetooth_service_uuid]
                        .empty()) {
                  service_uuid_to_session_ids_map_.erase(
                      bluetooth_service_uuid);
                }
                // Stop discovery if there's no more on-going scan sessions.
                if (session_id_to_scanning_callback_map_.empty()) {
                  bool stop_discovery_success = true;
                  if (discovery_session_) {
                    bool message_success =
                        discovery_session_->Stop(&stop_discovery_success);
                    stop_discovery_success =
                        stop_discovery_success && message_success;
                  }
                  adapter_observer_.reset();
                  discovery_session_.reset();
                  if (!stop_discovery_success) {
                    return absl::InternalError(
                        "Discovery is not fully stopped");
                  }
                }
                return absl::OkStatus();
              },
      });
}

std::unique_ptr<api::ble_v2::GattServer> BleV2Medium::StartGattServer(
    api::ble_v2::ServerGattConnectionCallback callback) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<api::ble_v2::GattClient> BleV2Medium::ConnectToGattServer(
    api::ble_v2::BlePeripheral& peripheral,
    api::ble_v2::TxPowerLevel tx_power_level,
    api::ble_v2::ClientGattConnectionCallback callback) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<api::ble_v2::BleServerSocket> BleV2Medium::OpenServerSocket(
    const std::string& service_id) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<api::ble_v2::BleSocket> BleV2Medium::Connect(
    const std::string& service_id,
    api::ble_v2::TxPowerLevel tx_power_level,
    api::ble_v2::BlePeripheral& peripheral,
    CancellationFlag* cancellation_flag) {
  NOTIMPLEMENTED();
  return nullptr;
}

bool BleV2Medium::IsExtendedAdvertisementsAvailable() {
  NOTIMPLEMENTED();
  return false;
}

bool BleV2Medium::GetRemotePeripheral(const std::string& mac_address,
                                      GetRemotePeripheralCallback callback) {
  NOTIMPLEMENTED();
  return false;
}

bool BleV2Medium::GetRemotePeripheral(api::ble_v2::BlePeripheral::UniqueId id,
                                      GetRemotePeripheralCallback callback) {
  NOTIMPLEMENTED();
  return false;
}

void BleV2Medium::PresentChanged(bool present) {
  NOTIMPLEMENTED();
}

void BleV2Medium::PoweredChanged(bool powered) {
  NOTIMPLEMENTED();
}

void BleV2Medium::DiscoverableChanged(bool discoverable) {
  NOTIMPLEMENTED();
}

void BleV2Medium::DiscoveringChanged(bool discovering) {
  if (!discovering) {
    StopScanning();
  }
}

void BleV2Medium::DeviceAdded(bluetooth::mojom::DeviceInfoPtr device) {
  if (!IsScanning()) {
    return;
  }

  // Best-effort attempt to filter out BT Classic devices. Dual-mode (BT
  // Classic and BLE) devices which the system has paired and/or connected to
  // may also expose service data, but all BLE advertisements that we are
  // interested in are captured in an element of |service_data_map|. See
  // BluetoothClassicMedium for separate discovery of BT Classic devices.
  if (device.is_null() || device->service_data_map.empty()) {
    return;
  }

  if (device.is_null()) {
    LOG(WARNING) << "Device is empty.";
    return;
  }

  // Extract Advertisement Data.
  auto advertisement_data = api::ble_v2::BleAdvertisementData{
      .is_extended_advertisement = false,
      .service_data = {},
  };
  base::flat_set<device::BluetoothUUID> bluetooth_service_set;
  for (const auto& service_data_pair : device->service_data_map) {
    bluetooth_service_set.insert(service_data_pair.first);
    advertisement_data.service_data.insert(
        {BluetoothServiceUuidToNearbyUuid(service_data_pair.first),
         ByteArray{std::string(service_data_pair.second.begin(),
                               service_data_pair.second.end())}});
  }

  // Add a new or update the existing discovered peripheral. Note: Because
  // BleV2Peripherals are passed by reference to NearbyConnections, if a
  // BleV2Peripheral already exists with the given address, the reference should
  // not be invalidated, the update functions should be called instead.
  const std::string& address = device->address;
  auto* existing_ble_peripheral = GetDiscoveredBlePeripheral(address);
  if (existing_ble_peripheral) {
    existing_ble_peripheral->UpdateDeviceInfo(std::move(device));
  } else {
    discovered_ble_peripherals_map_.emplace(
        address, chrome::BleV2Peripheral(std::move(device)));
  }

  for (const auto& service_uuid : bluetooth_service_set) {
    auto iter = service_uuid_to_session_ids_map_.find(service_uuid);
    if (iter == service_uuid_to_session_ids_map_.end()) {
      continue;
    }

    for (auto session_id : iter->second) {
      const auto scanning_callback_iter =
          session_id_to_scanning_callback_map_.find(session_id);
      if (scanning_callback_iter ==
          session_id_to_scanning_callback_map_.end()) {
        continue;
      }
      // Fetch the BleV2Peripheral with the same `address` again because
      // previously fetched pointers may have been invalidated while iterating
      // through the IDs.
      auto* ble_peripheral = GetDiscoveredBlePeripheral(address);
      if (!ble_peripheral) {
        LOG(WARNING) << "Can't find previously discovered ble peripheral.";
        continue;
      }

      if (scanning_callback_iter->second.advertisement_found_cb) {
        scanning_callback_iter->second.advertisement_found_cb(
            *ble_peripheral, advertisement_data);
      }
    }
  }
}

void BleV2Medium::DeviceChanged(bluetooth::mojom::DeviceInfoPtr device) {
  DeviceAdded(std::move(device));
}

void BleV2Medium::DeviceRemoved(bluetooth::mojom::DeviceInfoPtr device) {
  // TODO we also need this when productionize the ble medium code.
  NOTIMPLEMENTED();
}

bool BleV2Medium::IsScanning() {
  return adapter_observer_.is_bound() &&
         !service_uuid_to_session_ids_map_.empty() &&
         !session_id_to_scanning_callback_map_.empty();
}

chrome::BleV2Peripheral* BleV2Medium::GetDiscoveredBlePeripheral(
    const std::string& address) {
  auto it = discovered_ble_peripherals_map_.find(address);
  return it == discovered_ble_peripherals_map_.end() ? nullptr : &it->second;
}

uint64_t BleV2Medium::GenerateUniqueSessionId() {
  for (int i = 0; i < kGenerateSessionIdRetryLimit; i++) {
    uint64_t session_id = base::RandUint64();
    if (session_id != kFailedGenerateSessionId &&
        session_id_to_scanning_callback_map_.find(session_id) ==
            session_id_to_scanning_callback_map_.end()) {
      return session_id;
    }
  }
  return kFailedGenerateSessionId;
}

Uuid BleV2Medium::BluetoothServiceUuidToNearbyUuid(
    const device::BluetoothUUID& bluetooth_service_uuid) {
  auto uint_bytes = bluetooth_service_uuid.GetBytes();
  uint64_t most_sig_bits = 0;
  uint64_t least_sig_bits = 0;
  for (int i = 0; i < 8; i++) {
    most_sig_bits |= static_cast<uint64_t>(uint_bytes[i]) << ((7 - i) * 8);
    least_sig_bits |= static_cast<uint64_t>(uint_bytes[i + 8]) << ((7 - i) * 8);
  }
  return Uuid{most_sig_bits, least_sig_bits};
}
}  // namespace nearby::chrome
