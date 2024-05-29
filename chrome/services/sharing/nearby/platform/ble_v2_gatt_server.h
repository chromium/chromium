// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_GATT_SERVER_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_GATT_SERVER_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chrome/services/sharing/nearby/platform/bluetooth_adapter.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/nearby/src/internal/platform/implementation/ble_v2.h"

namespace nearby::chrome {

class BleV2GattServer : public ::nearby::api::ble_v2::GattServer,
                        public bluetooth::mojom::GattServiceObserver {
 public:
  // Representation of the local GattServices hosted in the BleV2GattServer.
  struct GattService {
    class Factory {
     public:
      virtual std::unique_ptr<GattService> Create();

      virtual ~Factory();
    };

    GattService();
    virtual ~GattService();
    GattService(GattService&) = delete;
    GattService& operator=(GattService&) = delete;

    mojo::SharedRemote<bluetooth::mojom::GattService> gatt_service_remote;
    base::flat_map<Uuid, api::ble_v2::GattCharacteristic>
        characteristic_uuid_to_characteristic_map;

    // Characteristic UUID to value map. The value is set
    // in `UpdateCharacteristic()`, and this class is responsible for storing
    // the value of a GATT characteristic. See documentation in
    // `UpdateCharacteristic()`.
    base::flat_map<Uuid, nearby::ByteArray> characteristic_uuid_to_value_map;
  };

  BleV2GattServer(const mojo::SharedRemote<bluetooth::mojom::Adapter>& adapter,
                  std::unique_ptr<GattService::Factory> gatt_service_factory =
                      std::make_unique<GattService::Factory>());
  ~BleV2GattServer() override;

  BleV2GattServer(const BleV2GattServer&) = delete;
  BleV2GattServer& operator=(const BleV2GattServer&) = delete;

  // nearby::api::ble_v2::GattServer:
  BluetoothAdapter& GetBlePeripheral() override;
  std::optional<api::ble_v2::GattCharacteristic> CreateCharacteristic(
      const Uuid& service_uuid,
      const Uuid& characteristic_uuid,
      api::ble_v2::GattCharacteristic::Permission permission,
      api::ble_v2::GattCharacteristic::Property property) override;
  bool UpdateCharacteristic(
      const api::ble_v2::GattCharacteristic& characteristic,
      const nearby::ByteArray& value) override;
  absl::Status NotifyCharacteristicChanged(
      const api::ble_v2::GattCharacteristic& characteristic,
      bool confirm,
      const nearby::ByteArray& new_value) override;
  void Stop() override;

  // Triggers registration of all `GattService`s known to this
  // `BleV2GattServer`, which will make each `GattService` and all of its
  // associated attributes available on the local adapters GATT database.
  // This should be called once all the requested characteristics have been
  // created via `CreateCharacteristic()`, before advertising begins.
  void RegisterGattServices(
      base::OnceCallback<void(bool)> on_registration_complete_callback);

  base::WeakPtr<BleV2GattServer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // bluetooth::mojom::GattServiceObserver:
  void OnLocalCharacteristicRead(
      bluetooth::mojom::DeviceInfoPtr remote_device,
      const device::BluetoothUUID& characteristic_uuid,
      const device::BluetoothUUID& service_uuid,
      uint32_t offset,
      OnLocalCharacteristicReadCallback callback) override;

  void RegisterGattService(GattService* gatt_service);
  void DoRegisterGattService(GattService* gatt_service);
  void OnRegisterGattService(
      std::optional<device::BluetoothGattService::GattErrorCode> error_code);

  void OnGattServiceDisconnected(const Uuid& gatt_service_id);

  // Indicates whether registration of this `BleV2GattServer` via `Register` has
  // been initiated. If so, this boolean helps enforce that calls to create
  // characteristics or add new `GattService`s are not supported, since they
  // will not show up on the platform layer to clients.
  bool has_registration_started_ = false;

  // Once registration has started (indicated by `has_registration_started_`),
  // represents the overall registration success of triggering registration on
  // the `GattService`s. If any `GattService` fails to be registered, a failure
  // is returned back on the callback in `Register()`. For the `BleV2GattServer`
  // MVP, only one `GattService` exists at a single time, since there is only
  // a single usecase for `GattService`, however, this allows this code to be
  // future-proofed and support multiple `GattService`s created at a time.
  bool did_any_gatt_services_fail_to_register_ = false;

  // `task_runner_` is required to prevent blocking in calls over the Mojo
  // remote during GATT service registration.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // `registration_barrier_` is initialized during `RegisterGattServices()`
  // with the number of `GattService`s that need to be registered via
  // `RegisterGattService()`. This is needed because `RegisterGattServices()`
  // kicks off multiple asynchronous tasks at once, and waits until they are
  // all completed. When a `GattService` successfully completes its
  // registration, it decrements the `registration_barrier_` count;
  // when all tasks are completed successfully, `BleV2GattServer` triggers the
  // passed in `on_registration_complete_callback_` to indicate to callers
  // success. When a `GattService` unsuccessfully completes its registration,
  // it immediately triggers the `on_registration_complete_callback_` with
  // failure, and resets the `registration_barrier_` since there is no need to
  // continue the registration of all other `GattService`s.
  std::unique_ptr<base::AtomicRefCount> registration_barrier_;
  base::OnceCallback<void(bool)> on_registration_complete_callback_;

  std::unique_ptr<GattService::Factory> gatt_service_factory_;
  std::unique_ptr<BluetoothAdapter> bluetooth_adapter_;
  base::flat_map<Uuid, std::unique_ptr<GattService>> uuid_to_gatt_service_map_;
  mojo::SharedRemote<bluetooth::mojom::Adapter> adapter_remote_;
  mojo::Receiver<bluetooth::mojom::GattServiceObserver> gatt_service_observer_{
      this};
  base::WeakPtrFactory<BleV2GattServer> weak_ptr_factory_{this};
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_GATT_SERVER_H_
