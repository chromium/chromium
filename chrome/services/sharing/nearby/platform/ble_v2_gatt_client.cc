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

  auto service_it =
      uuid_to_discovered_gatt_service_map_.find(std::string(service_uuid));
  if (service_it == uuid_to_discovered_gatt_service_map_.end()) {
    LOG(WARNING) << __func__ << ": no match for service at UUID"
                 << std::string(service_uuid) << " in "
                 << uuid_to_discovered_gatt_service_map_.size()
                 << " number of discovered services";
    return std::nullopt;
  }

  const auto& characteristics = service_it->second->characteristics;
  if (!characteristics.has_value()) {
    LOG(WARNING) << __func__ << ": characteristics have no value";
    return std::nullopt;
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
      ConvertPermission((*char_it)->permissions),
      ConvertProperty((*char_it)->properties)};
  return gatt_characteristic;
}

std::optional<std::string> BleV2GattClient::ReadCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic) {
  // TODO(b/311430390): Implement this function.
  NOTIMPLEMENTED();
  return std::nullopt;
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
  // TODO(b/311430390): For now, just tear down the connection when we call
  // Disconnect. In the future this should clean up state.
  remote_device_.reset();
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

  shutdown_waitable_event->Signal();
}

}  // namespace nearby::chrome
