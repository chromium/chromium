// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif
#include "chrome/services/sharing/nearby/platform/ble_v2_medium.h"

#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/services/sharing/nearby/platform/ble_v2_gatt_client.h"
#include "chrome/services/sharing/nearby/platform/ble_v2_gatt_server.h"
#include "chrome/services/sharing/nearby/platform/ble_v2_remote_peripheral.h"
#include "chrome/services/sharing/nearby/platform/ble_v2_server_socket.h"
#include "chrome/services/sharing/nearby/platform/bluetooth_utils.h"
#include "chrome/services/sharing/nearby/platform/nearby_platform_metrics.h"
#include "components/cross_device/logging/logging.h"
#include "components/cross_device/nearby/nearby_features.h"
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

std::string TxPowerLevelToName(api::ble_v2::TxPowerLevel tx_power_level) {
  switch (tx_power_level) {
    case api::ble_v2::TxPowerLevel::kUltraLow:
      return "UltraLow";
    case api::ble_v2::TxPowerLevel::kLow:
      return "Low";
    case api::ble_v2::TxPowerLevel::kMedium:
      return "Medium";
    case api::ble_v2::TxPowerLevel::kHigh:
      return "High";
    case api::ble_v2::TxPowerLevel::kUnknown:
      return "Unknown";
  }
}

void CancelPendingTasks(
    base::flat_set<raw_ptr<base::WaitableEvent>>& events_to_cancel) {
  if (!events_to_cancel.empty()) {
    DVLOG(1) << __func__ << ": Canceling " << events_to_cancel.size()
             << " pending calls.";
  }

  for (base::WaitableEvent* event : std::move(events_to_cancel)) {
    event->Signal();
  }
}

std::string_view ConnectResultToString(bluetooth::mojom::ConnectResult result) {
  switch (result) {
    case bluetooth::mojom::ConnectResult::SUCCESS:
      return "Success";
    case bluetooth::mojom::ConnectResult::AUTH_CANCELED:
      return "Auth Canceled";
    case bluetooth::mojom::ConnectResult::AUTH_FAILED:
      return "Auth Failed";
    case bluetooth::mojom::ConnectResult::AUTH_REJECTED:
      return "Auth Rejected";
    case bluetooth::mojom::ConnectResult::AUTH_TIMEOUT:
      return "Auth Timeout";
    case bluetooth::mojom::ConnectResult::FAILED:
      return "Failed";
    case bluetooth::mojom::ConnectResult::INPROGRESS:
      return "In Progress";
    case bluetooth::mojom::ConnectResult::UNKNOWN:
      return "Unknown";
    case bluetooth::mojom::ConnectResult::UNSUPPORTED_DEVICE:
      return "Unsupported Device";
    case bluetooth::mojom::ConnectResult::DEVICE_NO_LONGER_IN_RANGE:
      return "Device No Longer In Range";
    case bluetooth::mojom::ConnectResult::NOT_READY:
      return "Not Ready";
    case bluetooth::mojom::ConnectResult::ALREADY_CONNECTED:
      return "Already Connected";
    case bluetooth::mojom::ConnectResult::ALREADY_EXISTS:
      return "Already Exists";
    case bluetooth::mojom::ConnectResult::NOT_CONNECTED:
      return "Not Connected";
    case bluetooth::mojom::ConnectResult::DOES_NOT_EXIST:
      return "Does Not Exist";
    case bluetooth::mojom::ConnectResult::INVALID_ARGS:
      return "Invalid Args";
    case bluetooth::mojom::ConnectResult::NON_AUTH_TIMEOUT:
      return "Non Auth Timeout";
    case bluetooth::mojom::ConnectResult::NO_MEMORY:
      return "No Memory";
    case bluetooth::mojom::ConnectResult::JNI_ENVIRONMENT:
      return "JNI Environment";
    case bluetooth::mojom::ConnectResult::JNI_THREAD_ATTACH:
      return "JNI Thread Attach";
    case bluetooth::mojom::ConnectResult::WAKELOCK:
      return "Wakelock";
    case bluetooth::mojom::ConnectResult::UNEXPECTED_STATE:
      return "Unexpected State";
    case bluetooth::mojom::ConnectResult::SOCKET:
      return "Socket Error";
  }

  NOTREACHED();
}

}  // namespace

BleV2Medium::BleV2Medium(
    const mojo::SharedRemote<bluetooth::mojom::Adapter>& adapter)
    : task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      adapter_(adapter) {
  CHECK(adapter_.is_bound());
}

BleV2Medium::~BleV2Medium() {
  // For thread safety, shut down on the |task_runner_|.
  base::WaitableEvent shutdown_waitable_event;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BleV2Medium::Shutdown, base::Unretained(this),
                                &shutdown_waitable_event));
  shutdown_waitable_event.Wait();
}

bool BleV2Medium::StartAdvertising(
    const api::ble_v2::BleAdvertisementData& advertising_data,
    api::ble_v2::AdvertiseParameters advertise_set_parameters) {
  if (!features::IsNearbyBleV2Enabled()) {
    DVLOG(1) << __func__ << ": BleV2 is disabled.";
    return false;
  }

  // Before starting the advertising, register the GATT Services if supported
  // to make GATT advertisements available. To accommodate the asynchronous
  // nature of registering the GATT services via `RegisterGattServices()`,
  // block until registration succeeds or fails.
  if (gatt_server_) {
    DVLOG(1)
        << __func__
        << ": attempting to register GATT Services before starting advertising";

    base::WaitableEvent register_gatt_services_waitable_event;
    bool registration_success;

    // The `WeakPtr` to the `BleV2GattServer` cannot be dereferenced on a
    // different sequence than `StartAdvertising()` due to thread safety
    // enforcement in `WeakPtr`. Therefore, pass a raw pointer to the member
    // object to trigger registration.
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&BleV2Medium::DoRegisterGattServices,
                                  base::Unretained(this), gatt_server_.get(),
                                  &registration_success,
                                  &register_gatt_services_waitable_event));
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    register_gatt_services_waitable_event.Wait();

    if (!registration_success) {
      DLOG(WARNING)
          << __func__
          << ": failed register GATT Services before starting advertising; "
             "stopping advertising";
      metrics::RecordStartAdvertisingResult(
          /*success=*/false,
          /*is_extended_advertisement=*/false);
      metrics::RecordStartAdvertisingFailureReason(
          /*reason=*/metrics::StartAdvertisingFailureReason::
              kFailedToRegisterGattServices,
          /*is_extended_advertisement=*/false);
      return false;
    }
  }

  std::string service_data_info;
  for (auto it = advertising_data.service_data.begin();
       it != advertising_data.service_data.end(); it++) {
    service_data_info +=
        "{UUID:" + std::string(it->first) +
        ",data size:" + base::NumberToString(it->second.size()) + ",data=0x" +
        base::HexEncode(std::vector<uint8_t>(
            it->second.data(), it->second.data() + it->second.size())) +
        (std::next(it) == advertising_data.service_data.end() ? "}" : "}, ");
  }
  DVLOG(1) << __func__
           << "BLE_v2 StartAdvertising: "
              "advertising_data.is_extended_advertisement="
           << advertising_data.is_extended_advertisement
           << ", advertising_data.service_data=" << service_data_info
           << ", tx_power_level="
           << TxPowerLevelToName(advertise_set_parameters.tx_power_level)
           << ", is_connectable=" << advertise_set_parameters.is_connectable;

  if (advertising_data.is_extended_advertisement &&
      !IsExtendedAdvertisementsAvailable()) {
    // Nearby Connections is expected to pass us extended advertisements without
    // first checking if we have support. In that case we are expected to return
    // false.
    DLOG(WARNING) << __func__
                  << " Extended advertising is not supported, "
                     "not registering extended adv.";
    metrics::RecordStartAdvertisingResult(
        /*success=*/false,
        /*is_extended_advertisement=*/advertising_data
            .is_extended_advertisement);
    metrics::RecordStartAdvertisingFailureReason(
        /*reason=*/metrics::StartAdvertisingFailureReason::
            kNoExtendedAdvertisementSupport,
        /*is_extended_advertisement=*/advertising_data
            .is_extended_advertisement);
    return false;
  }

  // There are 3 types of advertisements that Nearby Connections will ask us
  // to broadcast. All 3 are connectable, but there are a few other
  // differences.
  // 1. Extended Advertisements - These do not have ScanResponse data, and
  //    contain their full payload in the AdvertisementData. This is limited by
  //    hardware support.
  // 2. Regular legacy GATT advertisements - These do use ScanResponse data.
  //    This can either contain real information about our GATT Server, or
  //    contain "dummy" info that signals that this device couldn't start the
  //    GATT Server (which is also limited by hardware support.)
  // 3. Fast advertisements - These do use ScanResponse data, and are shorter
  //    than GATT advertisements. These are expected to always be supported by
  //    hardware.
  std::map<device::BluetoothUUID,
           mojo::PendingRemote<bluetooth::mojom::Advertisement>>
      registered_advertisements;
  for (const auto& entry : advertising_data.service_data) {
    bool use_scan_response = true;
    if (advertising_data.is_extended_advertisement) {
      use_scan_response = false;
    }

    auto service_uuid = device::BluetoothUUID(std::string(entry.first));
    mojo::PendingRemote<bluetooth::mojom::Advertisement> pending_advertisement;
    bool success = adapter_->RegisterAdvertisement(
        service_uuid,
        std::vector<uint8_t>(entry.second.data(),
                             entry.second.data() + entry.second.size()),
        /*use_scan_data=*/use_scan_response,
        /*connectable=*/advertise_set_parameters.is_connectable,
        &pending_advertisement);

    if (!success || !pending_advertisement.is_valid()) {
      // Return early when failing to register an advertisement, even if
      // there are multiple sets of advertising data, as Nearby Connections
      // expects all advertisements to be registered on success.
      DLOG(WARNING) << __func__ << " Failed to register advertisement.";
      metrics::RecordStartAdvertisingResult(
          /*success=*/false,
          /*is_extended_advertisement=*/advertising_data
              .is_extended_advertisement);
      metrics::RecordStartAdvertisingFailureReason(
          /*reason=*/metrics::StartAdvertisingFailureReason::
              kAdapterRegisterAdvertisementFailed,
          /*is_extended_advertisement=*/advertising_data
              .is_extended_advertisement);
      return false;
    }

    registered_advertisements.emplace(service_uuid,
                                      std::move(pending_advertisement));
  }

  // Only save registered advertisements into the map after all registrations
  // succeed. Note that api::ble_v2::BleAdvertisementData enforces one
  // advertisement per UUID, but Nearby Connections expects us to handle
  // multiple registered advertisements per UUID.
  for (auto& entry : registered_advertisements) {
    registered_advertisements_map_[entry.first].emplace_back(
        std::move(entry.second), task_runner_);
  }

  DVLOG(1) << __func__ << " Started advertising.";
  metrics::RecordStartAdvertisingResult(
      /*success=*/true,
      /*is_extended_advertisement=*/advertising_data.is_extended_advertisement);
  return true;
}

std::unique_ptr<BleV2Medium::AdvertisingSession> BleV2Medium::StartAdvertising(
    const api::ble_v2::BleAdvertisementData& advertising_data,
    api::ble_v2::AdvertiseParameters advertise_set_parameters,
    BleV2Medium::AdvertisingCallback callback) {
  if (!features::IsNearbyBleV2Enabled()) {
    DVLOG(1) << __func__ << ": BleV2 is disabled.";
    return nullptr;
  }

  // TODO(b/318839357): deprecate the 'bool StartAdvertising' function.
  if (StartAdvertising(advertising_data, advertise_set_parameters)) {
    if (callback.start_advertising_result) {
      callback.start_advertising_result(absl::OkStatus());
    }
  } else {
    if (callback.start_advertising_result) {
      callback.start_advertising_result(
          absl::InternalError("Failed to start advertising."));
    }
    return nullptr;
  }

  return std::make_unique<BleV2Medium::AdvertisingSession>(
      BleV2Medium::AdvertisingSession{
          .stop_advertising =
              [this]() {
                if (StopAdvertising()) {
                  return absl::OkStatus();
                } else {
                  return absl::InternalError("Failed to stop advertising.");
                }
              },
      });
}

bool BleV2Medium::StopAdvertising() {
  if (!features::IsNearbyBleV2Enabled()) {
    DVLOG(1) << __func__ << ": BleV2 is disabled.";
    return false;
  }

  CD_LOG(INFO, Feature::NEARBY_INFRA)
      << __func__ << " Clearing registered advertisements.";
  registered_advertisements_map_.clear();
  return true;
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

std::unique_ptr<BleV2Medium::ScanningSession> BleV2Medium::StartScanning(
    const Uuid& service_uuid,
    api::ble_v2::TxPowerLevel tx_power_level,
    BleV2Medium::ScanningCallback callback) {
  if (!features::IsNearbyBleV2Enabled()) {
    DVLOG(1) << __func__ << ": BleV2 is disabled.";
    return nullptr;
  }

  if (!IsScanning()) {
    discovered_ble_peripherals_map_.clear();
    service_uuid_to_session_ids_map_.clear();
    session_id_to_scanning_callback_map_.clear();

    bool success =
        adapter_->AddObserver(adapter_observer_.BindNewPipeAndPassRemote());
    if (!success) {
      adapter_observer_.reset();
      metrics::RecordStartScanningResult(
          /*success=*/false);
      metrics::RecordStartScanningFailureReason(
          /*reason=*/metrics::StartScanningFailureReason::
              kAdapterObserverationFailed);
      return nullptr;
    }

    mojo::PendingRemote<bluetooth::mojom::DiscoverySession> discovery_session;
    success =
        adapter_->StartDiscoverySession(kScanClientName, &discovery_session);

    if (!success || !discovery_session.is_valid()) {
      adapter_observer_.reset();
      metrics::RecordStartScanningResult(
          /*success=*/false);
      metrics::RecordStartScanningFailureReason(
          /*reason=*/metrics::StartScanningFailureReason::
              kStartDiscoverySessionFailed);
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

  metrics::RecordStartScanningResult(
      /*success=*/true);

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
  if (!features::IsNearbyBleV2Enabled()) {
    DVLOG(1) << __func__ << ": BleV2 is disabled.";
    return nullptr;
  }

  if (!features::IsNearbyBleV2GattServerEnabled()) {
    return nullptr;
  }

  bool is_dual_role_supported;
  adapter_->IsLeScatternetDualRoleSupported(&is_dual_role_supported);
  metrics::RecordGattServerScatternetDualRoleSupported(is_dual_role_supported);
  if (!is_dual_role_supported) {
    return nullptr;
  }

  auto gatt_server = std::make_unique<BleV2GattServer>(adapter_);

  // TODO(b/335753061): Revisit the design pattern of holding onto a
  // `GattServer` pointer when Nearby Connections adjusts the BLE V2 APIs.
  gatt_server_ = gatt_server->GetWeakPtr();
  return gatt_server;
}

std::unique_ptr<api::ble_v2::GattClient> BleV2Medium::ConnectToGattServer(
    api::ble_v2::BlePeripheral& peripheral,
    api::ble_v2::TxPowerLevel tx_power_level,
    api::ble_v2::ClientGattConnectionCallback callback) {
  if (!features::IsNearbyBleV2Enabled()) {
    DVLOG(1) << __func__ << ": BleV2 is disabled.";
    return nullptr;
  }

  base::WaitableEvent connect_to_gatt_server_waitable_event;
  CHECK(adapter_.is_bound());
  mojo::PendingRemote<bluetooth::mojom::Device> device;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BleV2Medium::DoConnectToGattServer,
                     base::Unretained(this), &device, peripheral.GetAddress(),
                     &connect_to_gatt_server_waitable_event));
  base::ScopedAllowBaseSyncPrimitives allow_wait;
  connect_to_gatt_server_waitable_event.Wait();

  if (!device) {
    LOG(WARNING) << __func__ << ": could not connect to the GATT server";
    metrics::RecordConnectToRemoteGattServerResult(/*success=*/false);
    return nullptr;
  }

  metrics::RecordConnectToRemoteGattServerResult(/*success=*/true);

  // `tx_power_level` has no equivalent parameter in the Bluetooth Adapter
  // layer, so it is ignored.
  //
  // TODO(b/311430390): When Nearby Connections uses
  // `ClientGattConnectionCallback`, pass it into `BleV2GattClient` to trigger
  // events for characteristic subscription and disconnect.
  return std::make_unique<nearby::chrome::BleV2GattClient>(std::move(device));
}

std::unique_ptr<api::ble_v2::BleServerSocket> BleV2Medium::OpenServerSocket(
    const std::string& service_id) {
  if (!features::IsNearbyBleV2Enabled()) {
    DVLOG(1) << __func__ << ": BleV2 is disabled.";
    return nullptr;
  }

  // TODO(b/320554697): This function has no purpose in BLE V2 and can be
  // removed once implementation of the GATT Server advertising is complete.
  // Note that other platforms still use this function for now.
  return std::make_unique<BleV2ServerSocket>();
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
  if (!features::IsNearbyBleV2Enabled()) {
    DVLOG(1) << __func__ << ": BleV2 is disabled.";
    return false;
  }

  if (!features::IsNearbyBleV2ExtendedAdvertisingEnabled()) {
    return false;
  }

  bluetooth::mojom::AdapterInfoPtr info;
  bool success = adapter_->GetInfo(&info);
  return success && info->extended_advertisement_support;
}

bool BleV2Medium::GetRemotePeripheral(const std::string& mac_address,
                                      GetRemotePeripheralCallback callback) {
  NOTIMPLEMENTED();
  return false;
}

bool BleV2Medium::GetRemotePeripheral(api::ble_v2::BlePeripheral::UniqueId id,
                                      GetRemotePeripheralCallback callback) {
  auto it =
      std::find_if(discovered_ble_peripherals_map_.begin(),
                   discovered_ble_peripherals_map_.end(),
                   [&](const auto& address_device_pair) {
                     return address_device_pair.second.GetUniqueId() == id;
                   });

  if (it == discovered_ble_peripherals_map_.end()) {
    LOG(WARNING) << __func__ << ": no match for device at id = " << id;
    return false;
  }

  std::move(callback)(it->second);
  return true;
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
    CD_LOG(WARNING, Feature::NEARBY_INFRA) << __func__ << " Device is empty.";
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
        {BluetoothUuidToNearbyUuid(service_data_pair.first),
         ByteArray{std::string(service_data_pair.second.begin(),
                               service_data_pair.second.end())}});
  }

  // Add a new or update the existing discovered peripheral. Note: Because
  // BleV2RemotePeripherals are passed by reference to NearbyConnections, if a
  // BleV2RemotePeripheral already exists with the given address, the reference
  // should not be invalidated, the update functions should be called instead.
  const std::string& address = device->address;
  auto* existing_ble_peripheral = GetDiscoveredBlePeripheral(address);
  if (existing_ble_peripheral) {
    existing_ble_peripheral->UpdateDeviceInfo(std::move(device));
  } else {
    discovered_ble_peripherals_map_.emplace(
        address, chrome::BleV2RemotePeripheral(std::move(device)));
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
      // Fetch the BleV2RemotePeripheral with the same `address` again because
      // previously fetched pointers may have been invalidated while iterating
      // through the IDs.
      auto* ble_peripheral = GetDiscoveredBlePeripheral(address);
      if (!ble_peripheral) {
        CD_LOG(WARNING, Feature::NEARBY_INFRA)
            << __func__ << " Can't find previously discovered ble peripheral.";
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

chrome::BleV2RemotePeripheral* BleV2Medium::GetDiscoveredBlePeripheral(
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

void BleV2Medium::DoRegisterGattServices(
    BleV2GattServer* gatt_server,
    bool* registration_success,
    base::WaitableEvent* register_gatt_services_waitable_event) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  pending_register_gatt_services_waitable_events_.insert(
      register_gatt_services_waitable_event);

  CHECK(gatt_server);
  gatt_server->RegisterGattServices(base::BindOnce(
      &BleV2Medium::OnRegisterGattServices, base::Unretained(this),
      registration_success, register_gatt_services_waitable_event));
}

void BleV2Medium::OnRegisterGattServices(
    bool* out_registration_success,
    base::WaitableEvent* register_gatt_services_waitable_event,
    bool in_registration_success) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!pending_register_gatt_services_waitable_events_.contains(
          register_gatt_services_waitable_event)) {
    // The event has already been signaled.
    return;
  }

  *out_registration_success = in_registration_success;

  DVLOG(1) << "BleV2Medium::" << __func__
           << ": GATT Services registration result = "
           << (*out_registration_success ? "success" : "failure");

  if (!register_gatt_services_waitable_event->IsSignaled()) {
    register_gatt_services_waitable_event->Signal();
    pending_register_gatt_services_waitable_events_.erase(
        register_gatt_services_waitable_event);
  }
}

void BleV2Medium::DoConnectToGattServer(
    mojo::PendingRemote<bluetooth::mojom::Device>* device,
    const std::string& address,
    base::WaitableEvent* connect_to_gatt_server_waitable_event) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  pending_connect_to_gatt_server_waitable_events_.insert(
      connect_to_gatt_server_waitable_event);
  CHECK(adapter_.is_bound());
  adapter_->ConnectToDevice(
      address, base::BindOnce(
                   &BleV2Medium::OnConnectToGattServer, base::Unretained(this),
                   /*gatt_connection_start_time*/ base::TimeTicks::Now(),
                   device, connect_to_gatt_server_waitable_event));
}

void BleV2Medium::OnConnectToGattServer(
    base::TimeTicks gatt_connection_start_time,
    mojo::PendingRemote<bluetooth::mojom::Device>* out_device,
    base::WaitableEvent* connect_to_gatt_server_waitable_event,
    bluetooth::mojom::ConnectResult result,
    mojo::PendingRemote<bluetooth::mojom::Device> in_device) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!pending_connect_to_gatt_server_waitable_events_.contains(
          connect_to_gatt_server_waitable_event)) {
    // The event has already been signaled.
    return;
  }

  *out_device = std::move(in_device);

  VLOG(1) << __func__
          << ": ConnectToDevice() result = " << ConnectResultToString(result);

  if (result != bluetooth::mojom::ConnectResult::SUCCESS) {
    CHECK(!in_device);
    metrics::RecordConnectToRemoteGattServerFailureReason(result);
  } else {
    metrics::RecordConnectToRemoteGattServerDuration(
        /*duration=*/base::TimeTicks::Now() - gatt_connection_start_time);
  }

  if (!connect_to_gatt_server_waitable_event->IsSignaled()) {
    connect_to_gatt_server_waitable_event->Signal();
    pending_connect_to_gatt_server_waitable_events_.erase(
        connect_to_gatt_server_waitable_event);
  }
}

void BleV2Medium::Shutdown(base::WaitableEvent* shutdown_waitable_event) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  // Note that resetting the Remote will cancel any pending callbacks, including
  // those already in the task queue.
  gatt_server_.reset();
  adapter_.reset();
  discovery_session_.reset();
  discovered_ble_peripherals_map_.clear();
  session_id_to_scanning_callback_map_.clear();
  service_uuid_to_session_ids_map_.clear();
  registered_advertisements_map_.clear();

  // Cancel all pending connect/listen calls. This is sequence safe because all
  // changes to the pending-event sets are sequenced.
  CancelPendingTasks(pending_register_gatt_services_waitable_events_);
  CancelPendingTasks(pending_connect_to_gatt_server_waitable_events_);

  shutdown_waitable_event->Signal();
}

}  // namespace nearby::chrome
