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

void CancelPendingTasks(
    base::flat_set<raw_ptr<base::WaitableEvent>> events_to_cancel) {
  if (!events_to_cancel.empty()) {
    VLOG(1) << __func__ << ": Canceling " << events_to_cancel.size()
            << " pending calls.";
  }

  for (base::WaitableEvent* event : std::move(events_to_cancel)) {
    event->Signal();
  }

  events_to_cancel.clear();
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

  if (!base::Contains(uuid_to_discovered_gatt_service_map_,
                      std::string(service_uuid))) {
    LOG(WARNING) << __func__ << ": no match for " << std::string(service_uuid)
                 << " in " << uuid_to_discovered_gatt_service_map_.size()
                 << " number of services";
    return false;
  }

  // TODO(b/311430390): Kick off (2) discover GATT characteristics in the GATT
  // service at `characteristic_uuids`.
  return true;
}

std::optional<api::ble_v2::GattCharacteristic>
BleV2GattClient::GetCharacteristic(const Uuid& service_uuid,
                                   const Uuid& characteristic_uuid) {
  // TODO(b/311430390): Implement this function.
  NOTIMPLEMENTED();
  return std::nullopt;
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

  shutdown_waitable_event->Signal();
}

}  // namespace nearby::chrome
