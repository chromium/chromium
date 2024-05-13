// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_gatt_client.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/services/sharing/nearby/platform/bluetooth_utils.h"

namespace {

using BluetoothProperties = device::BluetoothGattCharacteristic::Properties;
using BluetoothPermissions = device::BluetoothGattCharacteristic::Permissions;
using BluetoothProperty = device::BluetoothGattCharacteristic::Property;
using BluetoothPermission = device::BluetoothGattCharacteristic::Permission;
using NearbyPermission = nearby::api::ble_v2::GattCharacteristic::Permission;
using NearbyProperty = nearby::api::ble_v2::GattCharacteristic::Property;

void CancelPendingTasks(
    base::flat_set<raw_ptr<base::WaitableEvent>>& events_to_cancel) {
  if (!events_to_cancel.empty()) {
    VLOG(1) << __func__ << ": Canceling " << events_to_cancel.size()
            << " pending calls.";
  }

  for (base::WaitableEvent* event : std::move(events_to_cancel)) {
    event->Signal();
  }
}

bool WereAllExpectedCharacteristicsFound(
    const std::vector<bluetooth::mojom::CharacteristicInfoPtr>&
        found_characteristics,
    const std::vector<nearby::Uuid>& expected_characteristic_uuids) {
  std::set<nearby::Uuid> found_characteristic_uuids;
  for (const auto& characteristic : found_characteristics) {
    found_characteristic_uuids.insert(
        nearby::chrome::BluetoothUuidToNearbyUuid(characteristic->uuid));
  }

  for (const auto& uuid : expected_characteristic_uuids) {
    if (found_characteristic_uuids.find(uuid) ==
        found_characteristic_uuids.end()) {
      return false;
    }
  }

  return true;
}

NearbyPermission ConvertPermission(BluetoothPermissions permissions) {
  if ((permissions & BluetoothPermission::PERMISSION_READ) != 0) {
    return NearbyPermission::kRead;
  }

  if ((permissions & BluetoothPermission::PERMISSION_WRITE) != 0) {
    return NearbyPermission::kWrite;
  }

  if ((permissions & BluetoothPermission::PERMISSION_WRITE) != 0) {
    return NearbyPermission::kWrite;
  }

  return NearbyPermission::kNone;
}

NearbyProperty ConvertProperty(BluetoothProperties properties) {
  if ((properties & BluetoothProperty::PROPERTY_READ) != 0) {
    return NearbyProperty::kRead;
  }

  if ((properties & BluetoothProperty::PROPERTY_WRITE) != 0) {
    return NearbyProperty::kWrite;
  }

  if ((properties & BluetoothProperty::PROPERTY_INDICATE) != 0) {
    return NearbyProperty::kIndicate;
  }

  if ((properties & BluetoothProperty::PROPERTY_NOTIFY) != 0) {
    return NearbyProperty::kNotify;
  }

  return NearbyProperty::kNone;
}

std::string_view GattResultToString(bluetooth::mojom::GattResult result) {
  switch (result) {
    case bluetooth::mojom::GattResult::SUCCESS:
      return "Success";
    case bluetooth::mojom::GattResult::UNKNOWN:
      return "Unknown";
    case bluetooth::mojom::GattResult::FAILED:
      return "Failed";
    case bluetooth::mojom::GattResult::IN_PROGRESS:
      return "In Progress";
    case bluetooth::mojom::GattResult::INVALID_LENGTH:
      return "Invalid Length";
    case bluetooth::mojom::GattResult::NOT_PERMITTED:
      return "Not Permitted";
    case bluetooth::mojom::GattResult::NOT_AUTHORIZED:
      return "Not Authorized";
    case bluetooth::mojom::GattResult::NOT_PAIRED:
      return "Not Paired";
    case bluetooth::mojom::GattResult::NOT_SUPPORTED:
      return "Not Supported";
    case bluetooth::mojom::GattResult::SERVICE_NOT_FOUND:
      return "Service Not Found";
    case bluetooth::mojom::GattResult::CHARACTERISTIC_NOT_FOUND:
      return "Characteristic Not Found";
    case bluetooth::mojom::GattResult::DESCRIPTOR_NOT_FOUND:
      return "Descriptor Not Found";
  }
}

}  // namespace

namespace nearby::chrome {

BleV2GattClient::BleV2GattClient(
    mojo::PendingRemote<bluetooth::mojom::Device> device)
    : task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      remote_device_(std::move(device), /*bind_task_runner=*/task_runner_) {
  CHECK(remote_device_.is_bound());
}

BleV2GattClient::~BleV2GattClient() {
  // For thread safety, shut down on the |task_runner_|.
  base::WaitableEvent shutdown_waitable_event;
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&BleV2GattClient::Shutdown,
                                                   base::Unretained(this),
                                                   &shutdown_waitable_event));
  shutdown_waitable_event.Wait();
}

bool BleV2GattClient::DiscoverServiceAndCharacteristics(
    const Uuid& service_uuid,
    const std::vector<Uuid>& characteristic_uuids) {
  VLOG(1) << __func__;

  // Nearby Connections expects `DiscoverServiceAndCharacteristics()` to be
  // a blocking call in order to (1) discover a GATT service at `service_uuid`
  // and (2) discover GATT characteristics in the GATT service at
  // `characteristic_uuids` in the GATT connection, represented by
  // `remote_device_`.
  //
  // First, kick off (1) Discover a GATT service at `service_uuid` by retrieving
  // all services on `remote_device_` if `BleV2GattClient` has not already, and
  // parsing for a match to  `service_uuid`. If there isn't a match, return a
  // failure.
  if (!have_gatt_services_been_discovered_) {
    VLOG(1) << __func__
            << ": attempting to discover services on the remote device";
    base::WaitableEvent discover_services_waitable_event;
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&BleV2GattClient::DoDiscoverServices,
                                          base::Unretained(this),
                                          &discover_services_waitable_event));
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    discover_services_waitable_event.Wait();
    have_gatt_services_been_discovered_ = true;
  }

  auto it =
      uuid_to_discovered_gatt_service_map_.find(std::string(service_uuid));
  if (it == uuid_to_discovered_gatt_service_map_.end()) {
    LOG(WARNING) << __func__ << ": no match for " << std::string(service_uuid)
                 << " in " << uuid_to_discovered_gatt_service_map_.size()
                 << " number of services";
    return false;
  }

  // Next, kick off (2) discover GATT characteristics in the GATT
  // service at `characteristic_uuids` by fetching all GATT characteristics in
  // `service_uuid` if we haven't already. Parse the list of GATT
  // characteristics for matches with all of the characteristic UUIDs in
  // `characteristic_uuids`; if not all match, return false.
  if (!it->second->characteristics.has_value()) {
    VLOG(1) << __func__
            << ": attempting to discover characteristics on the remote device "
               "for service uuid = "
            << std::string(service_uuid);
    base::WaitableEvent get_characteristics_waitable_event;
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&BleV2GattClient::DoGetCharacteristics,
                                  base::Unretained(this),
                                  /*gatt_service=*/it->second.get(),
                                  &get_characteristics_waitable_event));
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    get_characteristics_waitable_event.Wait();

    if (!it->second->characteristics.has_value()) {
      LOG(WARNING) << __func__
                   << ": failed to retrieve characteristics on the remote "
                      "device for service uuid = "
                   << std::string(service_uuid);
      return false;
    }
  }

  return WereAllExpectedCharacteristicsFound(
      /*found_characteristics=*/it->second->characteristics.value(),
      /*expected_characteristic_uuids=*/characteristic_uuids);
}

std::optional<api::ble_v2::GattCharacteristic>
BleV2GattClient::GetCharacteristic(const Uuid& service_uuid,
                                   const Uuid& characteristic_uuid) {
  VLOG(1) << __func__;

  auto characteristic_info =
      GetCharacteristicInfoMojom(service_uuid, characteristic_uuid);
  if (!characteristic_info) {
    return std::nullopt;
  }

  // The current implementation of BLE V2 in Nearby Connections only supports a
  // single permission or property type for a characteristic, even though the
  // Bluetooth Adapter in the platform layer can support multiple properties
  // using bitwise operations. `BleV2GattClient` converts the returned
  // properties and permissions to a single
  // nearby::api::ble_v2::GattCharacteristic::Property/Permission.
  api::ble_v2::GattCharacteristic gatt_characteristic = {
      characteristic_uuid, service_uuid,
      ConvertPermission(characteristic_info->permissions),
      ConvertProperty(characteristic_info->properties)};
  return gatt_characteristic;
}

std::optional<std::string> BleV2GattClient::ReadCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic) {
  auto characteristic_info = GetCharacteristicInfoMojom(
      characteristic.service_uuid, characteristic.uuid);
  if (!characteristic_info) {
    LOG(WARNING) << __func__
                 << ": no match for characteristic in the GATT client";
    return std::nullopt;
  }

  VLOG(1) << __func__ << ": attempting to read from characteristic at UUID = "
          << std::string(characteristic.uuid);

  std::optional<std::string> read_characteristic_result;
  base::WaitableEvent read_characteristic_waitable_event;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BleV2GattClient::DoReadCharacteristic, base::Unretained(this),
          /*service_id=*/
          uuid_to_discovered_gatt_service_map_
              .find(std::string(characteristic.service_uuid))
              ->second->service_info->id,
          /*characteristic_id=*/characteristic_info->id,
          &read_characteristic_result, &read_characteristic_waitable_event));
  base::ScopedAllowBaseSyncPrimitives allow_wait;
  read_characteristic_waitable_event.Wait();

  return read_characteristic_result;
}

bool BleV2GattClient::WriteCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic,
    std::string_view value,
    WriteType type) {
  // TODO(b/311430390): Implement this function.
  NOTIMPLEMENTED();
  return false;
}

bool BleV2GattClient::SetCharacteristicSubscription(
    const api::ble_v2::GattCharacteristic& characteristic,
    bool enable,
    absl::AnyInvocable<void(std::string_view value)>
        on_characteristic_changed_cb) {
  // TODO(b/311430390): Implement this function.
  NOTIMPLEMENTED();
  return false;
}

void BleV2GattClient::Disconnect() {
  remote_device_->Disconnect();
}

BleV2GattClient::GattService::GattService() = default;

BleV2GattClient::GattService::~GattService() = default;

void BleV2GattClient::DoDiscoverServices(
    base::WaitableEvent* discover_services_waitable_event) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  pending_discover_services_waitable_events_.insert(
      discover_services_waitable_event);
  CHECK(remote_device_.is_bound());

  remote_device_->GetServices(
      base::BindOnce(&BleV2GattClient::OnGetGattServices,
                     base::Unretained(this), discover_services_waitable_event));
}

void BleV2GattClient::OnGetGattServices(
    base::WaitableEvent* discover_services_waitable_event,
    std::vector<bluetooth::mojom::ServiceInfoPtr> services) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!pending_discover_services_waitable_events_.contains(
          discover_services_waitable_event)) {
    // The event has already been signaled.
    return;
  }

  VLOG(1) << __func__ << ": retrieved " << services.size()
          << ": GATT services from the remote device";

  // Store all returned `services` in case future calls to
  // `DiscoverServiceAndCharacteristics()` request a different service UUID to
  // prevent having to query the services again from the remote device.
  for (const auto& service : services) {
    Uuid nearby_service_uuid = BluetoothUuidToNearbyUuid(service->uuid);
    uuid_to_discovered_gatt_service_map_.insert_or_assign(
        std::string(nearby_service_uuid), std::make_unique<GattService>());
    uuid_to_discovered_gatt_service_map_.at(std::string(nearby_service_uuid))
        ->service_info = service.Clone();
  }

  if (!discover_services_waitable_event->IsSignaled()) {
    discover_services_waitable_event->Signal();
    pending_discover_services_waitable_events_.erase(
        discover_services_waitable_event);
  }
}

void BleV2GattClient::DoGetCharacteristics(
    GattService* gatt_service,
    base::WaitableEvent* get_characteristics_waitable_event) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK(remote_device_.is_bound());

  pending_get_characteristics_waitable_events_.insert(
      get_characteristics_waitable_event);
  remote_device_->GetCharacteristics(
      gatt_service->service_info->id,
      base::BindOnce(&BleV2GattClient::OnGetCharacteristics,
                     base::Unretained(this), gatt_service,
                     get_characteristics_waitable_event));
}

void BleV2GattClient::OnGetCharacteristics(
    GattService* gatt_service,
    base::WaitableEvent* get_characteristics_waitable_event,
    std::optional<std::vector<bluetooth::mojom::CharacteristicInfoPtr>>
        characteristics) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!pending_get_characteristics_waitable_events_.contains(
          get_characteristics_waitable_event)) {
    // The event has already been signaled.
    return;
  }

  if (characteristics.has_value()) {
    VLOG(1) << __func__ << ": got " << characteristics.value().size()
            << " characteristics";
    auto it = uuid_to_discovered_gatt_service_map_.find(std::string(
        BluetoothUuidToNearbyUuid(gatt_service->service_info->uuid)));
    CHECK(it != uuid_to_discovered_gatt_service_map_.end())
        << __func__
        << ": unexpectedly retrieved characteristics for a service that is "
           "unknown to BleV2GattClient";
    it->second->characteristics = std::move(characteristics);
  } else {
    LOG(WARNING) << __func__ << ": failed to get characteristics";
  }

  if (!get_characteristics_waitable_event->IsSignaled()) {
    get_characteristics_waitable_event->Signal();
    pending_get_characteristics_waitable_events_.erase(
        get_characteristics_waitable_event);
  }
}

void BleV2GattClient::DoReadCharacteristic(
    const std::string& service_id,
    const std::string& characteristic_id,
    std::optional<std::string>* read_characteristic_result,
    base::WaitableEvent* read_characteristic_waitable_event) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  pending_read_characteristic_waitable_events_.insert(
      read_characteristic_waitable_event);
  CHECK(remote_device_.is_bound());

  remote_device_->ReadValueForCharacteristic(
      service_id, characteristic_id,
      base::BindOnce(&BleV2GattClient::OnReadCharacteristic,
                     base::Unretained(this), read_characteristic_result,
                     read_characteristic_waitable_event));
}

void BleV2GattClient::OnReadCharacteristic(
    std::optional<std::string>* read_characteristic_result,
    base::WaitableEvent* read_characteristic_waitable_event,
    bluetooth::mojom::GattResult result,
    const std::optional<std::vector<uint8_t>>& value) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!pending_read_characteristic_waitable_events_.contains(
          read_characteristic_waitable_event)) {
    // The event has already been signaled.
    return;
  }

  VLOG(1) << __func__ << ": Result = " << GattResultToString(result);

  if (result != bluetooth::mojom::GattResult::SUCCESS) {
    CHECK(!value.has_value()) << __func__
                              << ": expected no value read from characteristic "
                                 "due to failure returned";
    *read_characteristic_result = std::nullopt;
  } else {
    CHECK(value.has_value())
        << __func__
        << ": expected value read from characteristic due to success returned";
    *read_characteristic_result =
        std::string(value.value().begin(), value.value().end());
  }

  if (!read_characteristic_waitable_event->IsSignaled()) {
    read_characteristic_waitable_event->Signal();
    pending_read_characteristic_waitable_events_.erase(
        read_characteristic_waitable_event);
  }
}

bluetooth::mojom::CharacteristicInfoPtr
BleV2GattClient::GetCharacteristicInfoMojom(const Uuid& service_uuid,
                                            const Uuid& characteristic_uuid) {
  VLOG(1) << __func__;

  auto service_it =
      uuid_to_discovered_gatt_service_map_.find(std::string(service_uuid));
  if (service_it == uuid_to_discovered_gatt_service_map_.end()) {
    LOG(WARNING) << __func__ << ": no match for service at UUID"
                 << std::string(service_uuid) << " in "
                 << uuid_to_discovered_gatt_service_map_.size()
                 << " number of discovered services";
    return nullptr;
  }

  const auto& characteristics = service_it->second->characteristics;
  if (!characteristics.has_value()) {
    LOG(WARNING) << __func__ << ": characteristics have no value";
    return nullptr;
  }

  auto char_it = std::find_if(
      characteristics.value().begin(), characteristics.value().end(),
      [characteristic_uuid](
          const bluetooth::mojom::CharacteristicInfoPtr& characteristic_info) {
        return characteristic_uuid ==
               BluetoothUuidToNearbyUuid(characteristic_info->uuid);
      });
  if (char_it == characteristics.value().end()) {
    LOG(WARNING) << __func__ << ": no match for characteristic at UUID"
                 << std::string(characteristic_uuid) << " in "
                 << characteristics.value().size()
                 << " number of discovered characteristics";
    return nullptr;
  }

  return char_it->Clone();
}

void BleV2GattClient::Shutdown(base::WaitableEvent* shutdown_waitable_event) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  Disconnect();

  // Note that resetting the Remote will cancel any pending callbacks, including
  // those already in the task queue.
  remote_device_.reset();
  uuid_to_discovered_gatt_service_map_.clear();

  // Cancel all pending calls. This is sequence safe because all
  // changes to the pending-event sets are sequenced. Make a copy of the events
  // because elements will be removed from the sets during iteration.
  CancelPendingTasks(pending_discover_services_waitable_events_);
  CancelPendingTasks(pending_get_characteristics_waitable_events_);
  CancelPendingTasks(pending_read_characteristic_waitable_events_);

  shutdown_waitable_event->Signal();
}

}  // namespace nearby::chrome
