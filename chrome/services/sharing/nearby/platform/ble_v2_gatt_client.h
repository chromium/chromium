// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_GATT_CLIENT_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_GATT_CLIENT_H_

#include "device/bluetooth/public/mojom/device.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/nearby/src/internal/platform/implementation/ble_v2.h"

namespace base {
class SequencedTaskRunner;
class WaitableEvent;
}  // namespace base

namespace nearby::chrome {

class BleV2GattClient : public ::nearby::api::ble_v2::GattClient {
 public:
  explicit BleV2GattClient(
      mojo::PendingRemote<bluetooth::mojom::Device> device);
  ~BleV2GattClient() override;

  BleV2GattClient(const BleV2GattClient&) = delete;
  BleV2GattClient& operator=(const BleV2GattClient&) = delete;

  // nearby::api::ble_v2::GattClient:
  bool DiscoverServiceAndCharacteristics(
      const Uuid& service_uuid,
      const std::vector<Uuid>& characteristic_uuids) override;
  std::optional<api::ble_v2::GattCharacteristic> GetCharacteristic(
      const Uuid& service_uuid,
      const Uuid& characteristic_uuid) override;
  std::optional<std::string> ReadCharacteristic(
      const api::ble_v2::GattCharacteristic& characteristic) override;
  bool WriteCharacteristic(
      const api::ble_v2::GattCharacteristic& characteristic,
      std::string_view value,
      WriteType type) override;
  bool SetCharacteristicSubscription(
      const api::ble_v2::GattCharacteristic& characteristic,
      bool enable,
      absl::AnyInvocable<void(std::string_view value)>
          on_characteristic_changed_cb) override;
  void Disconnect() override;

 private:
  struct GattService {
    GattService();
    ~GattService();

    bluetooth::mojom::ServiceInfoPtr service_info;
    std::optional<std::vector<bluetooth::mojom::CharacteristicInfoPtr>>
        characteristics;
  };

  void DoDiscoverServices(
      base::WaitableEvent* discover_services_waitable_event);
  void OnGetGattServices(
      base::WaitableEvent* discover_services_waitable_event,
      std::vector<bluetooth::mojom::ServiceInfoPtr> services);

  void DoGetCharacteristics(
      GattService* gatt_service,
      base::WaitableEvent* get_characteristics_waitable_event);
  void OnGetCharacteristics(
      GattService* gatt_service,
      base::WaitableEvent* get_characteristics_waitable_event,
      std::optional<std::vector<bluetooth::mojom::CharacteristicInfoPtr>>
          characteristics);

  void DoReadCharacteristic(
      const std::string& service_id,
      const std::string& characteristic_id,
      std::optional<std::string>* read_characteristic_result,
      base::WaitableEvent* read_characteristic_waitable_event);
  void OnReadCharacteristic(
      std::optional<std::string>* read_characteristic_result,
      base::WaitableEvent* read_characteristic_waitable_event,
      bluetooth::mojom::GattResult result,
      const std::optional<std::vector<uint8_t>>& value);

  bluetooth::mojom::CharacteristicInfoPtr GetCharacteristicInfoMojom(
      const Uuid& service_uuid,
      const Uuid& characteristic_uuid);

  void Shutdown(base::WaitableEvent* shutdown_waitable_event);

  bool have_gatt_services_been_discovered_ = false;
  std::map<std::string, std::unique_ptr<GattService>>
      uuid_to_discovered_gatt_service_map_;

  // Track all pending tasks in case the object is invalidated while
  // waiting.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::flat_set<raw_ptr<base::WaitableEvent>>
      pending_discover_services_waitable_events_;
  base::flat_set<raw_ptr<base::WaitableEvent>>
      pending_get_characteristics_waitable_events_;
  base::flat_set<raw_ptr<base::WaitableEvent>>
      pending_read_characteristic_waitable_events_;

  mojo::SharedRemote<bluetooth::mojom::Device> remote_device_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_GATT_CLIENT_H_
