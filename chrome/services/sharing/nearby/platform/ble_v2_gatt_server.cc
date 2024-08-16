// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/services/sharing/nearby/platform/ble_v2_gatt_server.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/services/sharing/nearby/platform/bluetooth_utils.h"
#include "chrome/services/sharing/nearby/platform/count_down_latch.h"
#include "chrome/services/sharing/nearby/platform/nearby_platform_metrics.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
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
      NOTREACHED();
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
      NOTREACHED();
  }
}

std::string_view GattErrorCodeToString(
    device::BluetoothGattService::GattErrorCode error_code) {
  switch (error_code) {
    case device::BluetoothGattService::GattErrorCode::kUnknown:
      return "Unknown";
    case device::BluetoothGattService::GattErrorCode::kFailed:
      return "Failed";
    case device::BluetoothGattService::GattErrorCode::kInProgress:
      return "In Progress";
    case device::BluetoothGattService::GattErrorCode::kInvalidLength:
      return "Invalid Length";
    case device::BluetoothGattService::GattErrorCode::kNotPermitted:
      return "Not Permitted";
    case device::BluetoothGattService::GattErrorCode::kNotAuthorized:
      return "Not Authorized";
    case device::BluetoothGattService::GattErrorCode::kNotPaired:
      return "Not Paired";
    case device::BluetoothGattService::GattErrorCode::kNotSupported:
      return "Not Supported";
  }
}

}  // namespace

namespace nearby::chrome {

std::unique_ptr<BleV2GattServer::GattService>
BleV2GattServer::GattService::Factory::Create() {
  return base::WrapUnique(new BleV2GattServer::GattService());
}

BleV2GattServer::GattService::Factory::~Factory() = default;

BleV2GattServer::GattService::GattService() = default;
BleV2GattServer::GattService::~GattService() = default;

BleV2GattServer::BleV2GattServer(
    const mojo::SharedRemote<bluetooth::mojom::Adapter>& adapter,
    std::unique_ptr<GattService::Factory> gatt_service_factory)
    : task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      gatt_service_factory_(std::move(gatt_service_factory)),
      bluetooth_adapter_(std::make_unique<BluetoothAdapter>(adapter)),
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
  // Characteristics can only be created and added to a `GattService` before
  // registration has begun.
  CHECK(!has_registration_started_);

  VLOG(1) << __func__;

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
      metrics::RecordCreateLocalGattServiceResult(/*success=*/false);
      return std::nullopt;
    }

    metrics::RecordCreateLocalGattServiceResult(/*success=*/true);
    auto gatt_service = gatt_service_factory_->Create();
    gatt_service->gatt_service_remote.Bind(
        std::move(gatt_service_pending_remote),
        /*bind_task_runner=*/task_runner_);
    gatt_service->gatt_service_remote.set_disconnect_handler(
        base::BindOnce(&BleV2GattServer::OnGattServiceDisconnected,
                       base::Unretained(this), service_uuid),
        /*handler_task_runner=*/task_runner_);
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
  device::BluetoothUUID bluetooth_characteristic_uuid{
      std::string(characteristic_uuid)};
  gatt_service->gatt_service_remote->CreateCharacteristic(
      /*characteristic_uuid=*/bluetooth_characteristic_uuid,
      /*permissions=*/ConvertPermission(permission),
      /*properties=*/ConvertProperty(property),
      /*out_success=*/&create_characteristic_success);

  if (!create_characteristic_success) {
    LOG(WARNING) << __func__ << ": Unable to create GATT characteristic";
    metrics::RecordCreateLocalGattCharacteristicResult(/*success=*/false);
    return std::nullopt;
  }

  // If successful in creating the GATT characteristic, create a corresponding
  // representation of the GATT characteristic to return back to the Nearby
  // Connections library. This will be used to trigger requests to notify or
  // update the GATT characteristic in other methods. The browser process
  // retrieves the corresponding GATT characteristic by `charactertistic_uuid`.
  metrics::RecordCreateLocalGattCharacteristicResult(/*success=*/true);
  api::ble_v2::GattCharacteristic gatt_characteristic = {
      characteristic_uuid, service_uuid, permission, property};
  gatt_service->characteristic_uuid_to_characteristic_map.insert_or_assign(
      characteristic_uuid, gatt_characteristic);
  return gatt_characteristic;
}

bool BleV2GattServer::UpdateCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic,
    const nearby::ByteArray& value) {
  VLOG(1) << __func__;

  auto service_it = uuid_to_gatt_service_map_.find(characteristic.service_uuid);
  if (service_it == uuid_to_gatt_service_map_.end()) {
    LOG(WARNING) << __func__
                 << ": trying to update a characteristic in a service that "
                    "doesn't exist";
    metrics::RecordUpdateCharacteristicResult(/*success=*/false);
    return false;
  }

  auto* gatt_service = service_it->second.get();
  const auto& char_map =
      gatt_service->characteristic_uuid_to_characteristic_map;
  auto char_it = char_map.find(characteristic.uuid);
  if (char_it == char_map.end()) {
    LOG(WARNING) << __func__
                 << ": trying to update a characteristic that doesn't exist in "
                    "the GATT service";
    metrics::RecordUpdateCharacteristicResult(/*success=*/false);
    return false;
  }

  // //device/bluetooth is not responsible for storing the value of a GATT
  // characteristic -- it is the responsibility of the `GattService`'s Delegate.
  // The `GattService` will relay corresponding messages on its Delegate
  // to `BleV2GattServer`, so the `BleV2GattServer` is responsible for storing
  // the value of the GATT characteristic and providing it when a read
  // is requested by a GATT client in `OnLocalCharacteristicRead()`.
  VLOG(1) << __func__ << ": storing value for a characteristic at UUID = "
          << characteristic.uuid.Get16BitAsString();
  gatt_service->characteristic_uuid_to_value_map.emplace(characteristic.uuid,
                                                         value);
  metrics::RecordUpdateCharacteristicResult(/*success=*/true);
  return true;
}

absl::Status BleV2GattServer::NotifyCharacteristicChanged(
    const api::ble_v2::GattCharacteristic& characteristic,
    bool confirm,
    const nearby::ByteArray& new_value) {
  // TODO(b/311430390): Implement to call on the Mojo remote to update the value
  // of the GATT Characteristic, and notify remote devices, and returns the
  // resulting Status of the operation. When implementing, consider if
  // `UpdateCharacteristic()` needs to be called before calling
  // `NotifyCharacteristicChanged()` in the library, or in this class, so
  // `BleV2GattServer` holds onto the updated value for any READ requests.
  NOTIMPLEMENTED();
  return absl::Status();
}

void BleV2GattServer::Stop() {
  VLOG(1) << __func__;

  // Clearing the `uuid_to_gatt_service_map_` destroys all `GattService`s owned
  // by `BleV2GattServer`, which also includes destroying their underlying
  // `GattService` Mojo remotes.
  uuid_to_gatt_service_map_.clear();
}

void BleV2GattServer::RegisterGattServices(
    base::OnceCallback<void(bool)> on_registration_complete_callback) {
  // `RegisterGattServices()` is expected to only be called once during the
  // lifetime of its class to kick off registration of the GATT services.
  CHECK(!has_registration_started_);
  has_registration_started_ = true;

  VLOG(1) << __func__;

  if (uuid_to_gatt_service_map_.empty()) {
    VLOG(1) << __func__ << ": no GATT services to register; returning success";
    std::move(on_registration_complete_callback_).Run(/*success=*/true);
    return;
  }

  on_registration_complete_callback_ =
      std::move(on_registration_complete_callback);
  registration_barrier_ =
      std::make_unique<base::AtomicRefCount>(uuid_to_gatt_service_map_.size());

  for (auto it = uuid_to_gatt_service_map_.begin();
       it != uuid_to_gatt_service_map_.end(); it++) {
    DoRegisterGattService(it->second.get());
  }
}

void BleV2GattServer::DoRegisterGattService(GattService* gatt_service) {
  CHECK(gatt_service->gatt_service_remote.is_bound());
  gatt_service->gatt_service_remote->Register(base::BindOnce(
      &BleV2GattServer::OnRegisterGattService, base::Unretained(this)));
}

void BleV2GattServer::OnRegisterGattService(
    std::optional<device::BluetoothGattService::GattErrorCode> error_code) {
  // If there are multiple GATT services being registered, even though the
  // GATT server ultimately returns failure if any single one fails to be
  // registered, we continue the registration of all the GATT services and
  // do not early return to prevent a case where a GATT service is
  // destroyed before its registration completes asynchronously.
  if (error_code) {
    LOG(WARNING) << __func__ << ": failed due to error = "
                 << GattErrorCodeToString(*error_code);
    did_any_gatt_services_fail_to_register_ = true;
    metrics::RecordGattServiceRegistrationErrorReason(error_code.value());
  }

  if (!registration_barrier_->Decrement()) {
    VLOG(1) << __func__ << ": registration result = "
            << (!did_any_gatt_services_fail_to_register_ ? "success"
                                                         : "failure");
    metrics::RecordGattServiceRegistrationResult(
        /*success=*/!did_any_gatt_services_fail_to_register_);
    CHECK(on_registration_complete_callback_);
    std::move(on_registration_complete_callback_)
        .Run(/*success=*/!did_any_gatt_services_fail_to_register_);
  }
}

void BleV2GattServer::OnLocalCharacteristicRead(
    bluetooth::mojom::DeviceInfoPtr remote_device,
    const device::BluetoothUUID& characteristic_uuid,
    const device::BluetoothUUID& service_uuid,
    uint32_t offset,
    OnLocalCharacteristicReadCallback callback) {
  VLOG(1) << __func__;

  Uuid nearby_service_uuid = BluetoothUuidToNearbyUuid(service_uuid);
  Uuid nearby_characteristic_uuid =
      BluetoothUuidToNearbyUuid(characteristic_uuid);

  // Expect that `OnLocalCharacteristicRead()` is called for a
  // characteristic that already exists in the `uuid_to_gatt_service_map_` of
  // the corresponding `GattService`. If this isn't true, it means the
  // corresponding GATT service in the browser process and this
  // `BleV2GattServer` have gotten out of sync.
  auto service_it = uuid_to_gatt_service_map_.find(nearby_service_uuid);
  CHECK(service_it != uuid_to_gatt_service_map_.end());
  auto* gatt_service = service_it->second.get();
  const auto& char_map =
      gatt_service->characteristic_uuid_to_characteristic_map;
  auto char_it = char_map.find(nearby_characteristic_uuid);
  CHECK(char_it != char_map.end());

  // Return an error if the property and permission of the characteristic does
  // not support read requests. `nearby::api::ble_v2::GattCharacteristic` only
  // support a single property and permission.
  if ((char_it->second.property !=
       nearby::api::ble_v2::GattCharacteristic::Property::kRead) ||
      (char_it->second.permission !=
       nearby::api::ble_v2::GattCharacteristic::Permission::kRead)) {
    LOG(WARNING) << __func__
                 << ": trying to read a characteristic that does not support "
                    "read requests";
    metrics::RecordOnLocalCharacteristicReadResult(/*success=*/false);
    std::move(callback).Run(
        bluetooth::mojom::LocalCharacteristicReadResult::NewErrorCode(
            device::BluetoothGattService::GattErrorCode::kNotPermitted));
    return;
  }

  // When a characteristic has a value set with
  // `BleV2GattServer::UpdateCharacteristic()`, then reading from the
  // characteristic yields that value. If there isn't a value in the
  // map for this characteristic, it means that it wasn't set correctly by
  // callers of `BleV2GattServer`.
  const auto& new_value_map = gatt_service->characteristic_uuid_to_value_map;
  auto new_value_it = new_value_map.find(nearby_characteristic_uuid);
  if (new_value_it == new_value_map.end()) {
    LOG(WARNING) << __func__
                 << ": value for the characteristic read request not found";
    metrics::RecordOnLocalCharacteristicReadResult(/*success=*/false);
    std::move(callback).Run(
        bluetooth::mojom::LocalCharacteristicReadResult::NewErrorCode(
            device::BluetoothGattService::GattErrorCode::kNotSupported));
    return;
  }

  const ByteArray& data = new_value_it->second;
  if (offset >= data.size()) {
    LOG(WARNING) << __func__ << ": invalid offset";
    std::move(callback).Run(
        bluetooth::mojom::LocalCharacteristicReadResult::NewErrorCode(
            device::BluetoothGattService::GattErrorCode::kInvalidLength));
    return;
  }

  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data.data());
  std::vector<uint8_t> read_value(bytes + offset, bytes + data.size());
  metrics::RecordOnLocalCharacteristicReadResult(/*success=*/true);
  std::move(callback).Run(
      bluetooth::mojom::LocalCharacteristicReadResult::NewData(
          std::move(read_value)));
}

void BleV2GattServer::OnGattServiceDisconnected(const Uuid& gatt_service_id) {
  LOG(WARNING) << __func__ << ": GATT service at "
               << gatt_service_id.Get16BitAsString()
               << ": unexpectedly disconnected.";

  uuid_to_gatt_service_map_.erase(gatt_service_id);
}

}  // namespace nearby::chrome
