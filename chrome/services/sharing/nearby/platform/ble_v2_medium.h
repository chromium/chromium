// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_MEDIUM_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_MEDIUM_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/services/sharing/nearby/platform/ble_v2_remote_peripheral.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/nearby/src/internal/platform/implementation/ble_v2.h"

namespace base {
class SequencedTaskRunner;
class WaitableEvent;
}  // namespace base

namespace nearby::chrome {

class BleV2GattServer;

// Concrete ble_v2::BleMedium implementation.
// Productionized impl will also consumes bluetooth::mojom::Adapter methods.
// Current skeleton has not added it yet.
class BleV2Medium : public ::nearby::api::ble_v2::BleMedium,
                    public bluetooth::mojom::AdapterObserver {
 public:
  BleV2Medium();

  explicit BleV2Medium(
      const mojo::SharedRemote<bluetooth::mojom::Adapter>& adapter);

  ~BleV2Medium() override;

  BleV2Medium(const BleV2Medium&) = delete;
  BleV2Medium& operator=(const BleV2Medium&) = delete;

  // This would deprecate soon. Will delete this after nearby codes delete it
  // first.
  // TODO(b/271305977): Delete this method.
  bool StartAdvertising(
      const api::ble_v2::BleAdvertisementData& advertising_data,
      api::ble_v2::AdvertiseParameters advertise_set_parameters) override;

  std::unique_ptr<AdvertisingSession> StartAdvertising(
      const api::ble_v2::BleAdvertisementData& advertising_data,
      api::ble_v2::AdvertiseParameters advertise_set_parameters,
      AdvertisingCallback callback) override;

  // This would deprecate soon. Will delete this after nearby codes delete it
  // first.
  // TODO(b/271305977): Delete this method.
  bool StopAdvertising() override;

  // This would deprecate soon. Will delete this after nearby codes delete it
  // first.
  // TODO(b/271305977): Delete this method.
  bool StartScanning(const Uuid& service_uuid,
                     api::ble_v2::TxPowerLevel tx_power_level,
                     ScanCallback callback) override;

  // This would deprecate soon. Will delete this after nearby codes delete it
  // first.
  // TODO(b/271305977): Delete this method.
  bool StopScanning() override;

  std::unique_ptr<ScanningSession> StartScanning(
      const Uuid& service_uuid,
      api::ble_v2::TxPowerLevel tx_power_level,
      ScanningCallback callback) override;

  std::unique_ptr<api::ble_v2::GattServer> StartGattServer(
      api::ble_v2::ServerGattConnectionCallback callback) override;

  std::unique_ptr<api::ble_v2::GattClient> ConnectToGattServer(
      api::ble_v2::BlePeripheral& peripheral,
      api::ble_v2::TxPowerLevel tx_power_level,
      api::ble_v2::ClientGattConnectionCallback callback) override;

  std::unique_ptr<api::ble_v2::BleServerSocket> OpenServerSocket(
      const std::string& service_id) override;

  std::unique_ptr<api::ble_v2::BleSocket> Connect(
      const std::string& service_id,
      api::ble_v2::TxPowerLevel tx_power_level,
      api::ble_v2::BlePeripheral& peripheral,
      CancellationFlag* cancellation_flag) override;

  bool IsExtendedAdvertisementsAvailable() override;

  bool GetRemotePeripheral(const std::string& mac_address,
                           GetRemotePeripheralCallback callback) override;
  bool GetRemotePeripheral(api::ble_v2::BlePeripheral::UniqueId id,
                           GetRemotePeripheralCallback callback) override;

 private:
  // TODO(b/330759317): Remove FRIEND call once unit tests can rely on
  // Fake classes instead of accessing private members.
  FRIEND_TEST_ALL_PREFIXES(BleV2MediumTest,
                           TestAdvertising_MultipleStartAdvertisingSuccess);
  // bluetooth::mojom::AdapterObserver:
  void PresentChanged(bool present) override;
  void PoweredChanged(bool powered) override;
  void DiscoverableChanged(bool discoverable) override;
  void DiscoveringChanged(bool discovering) override;
  void DeviceAdded(bluetooth::mojom::DeviceInfoPtr device) override;
  void DeviceChanged(bluetooth::mojom::DeviceInfoPtr device) override;
  void DeviceRemoved(bluetooth::mojom::DeviceInfoPtr device) override;

  void ProcessFoundDevice(bluetooth::mojom::DeviceInfoPtr device);
  bool IsScanning();
  uint64_t GenerateUniqueSessionId();
  chrome::BleV2RemotePeripheral* GetDiscoveredBlePeripheral(
      const std::string& address);

  void DoRegisterGattServices(
      BleV2GattServer* gatt_server,
      bool* registration_success,
      base::WaitableEvent* register_gatt_services_waitable_event);
  void OnRegisterGattServices(
      bool* out_registration_success,
      base::WaitableEvent* register_gatt_services_waitable_event,
      bool in_registration_success);

  void DoConnectToGattServer(
      mojo::PendingRemote<bluetooth::mojom::Device>* device,
      const std::string& address,
      base::WaitableEvent* connect_to_gatt_server_waitable_event);
  void OnConnectToGattServer(
      base::TimeTicks gatt_connection_start_time,
      mojo::PendingRemote<bluetooth::mojom::Device>* out_device,
      base::WaitableEvent* connect_to_gatt_server_waitable_event,
      bluetooth::mojom::ConnectResult result,
      mojo::PendingRemote<bluetooth::mojom::Device> in_device);

  void Shutdown(base::WaitableEvent* shutdown_waitable_event);

  // `task_runner_` is used in `StartAdvertising()` to post a task to register
  // `ble_v2_gatt_server_` if it exists (which indicates that the GATT server
  // will be advertised, thus all the GATT services associated with
  // `ble_v2_gatt_server_` need to be registered beforehand). The
  // `task_runner_` and posted tasks are shutdown in `Shutdown()` in the
  // destructor.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  mojo::SharedRemote<bluetooth::mojom::Adapter> adapter_;

  // Only set in `StartGattServer()` if the LE Dual Scatternet role
  // is supported on the device. This is used to trigger asynchronous GATT
  // server registration in `StartAdvertising()`.
  base::WeakPtr<BleV2GattServer> gatt_server_;

  // Track all pending register GATT service waitable events.
  base::flat_set<raw_ptr<base::WaitableEvent>>
      pending_register_gatt_services_waitable_events_;

  // Track all pending connect to remote GATT servers waitable events.
  base::flat_set<raw_ptr<base::WaitableEvent>>
      pending_connect_to_gatt_server_waitable_events_;

  // Only set while discovery is active.
  mojo::Remote<bluetooth::mojom::DiscoverySession> discovery_session_;

  // Group scanning session ids with the same bluetooth service id in sets.
  std::map<device::BluetoothUUID, std::set<uint64_t>>
      service_uuid_to_session_ids_map_;

  // Saving scanning callbacks, keyed by session ids.
  std::map<uint64_t, ScanningCallback> session_id_to_scanning_callback_map_;

  // Keyed by address of advertising remote device. Note: References to these
  // BlePeripherals are passed to Nearby Connections. This is safe because, for
  // std::map, insert/emplace do not invalidate references, and the erase
  // operation only invalidates the reference to the erased element.
  std::map<std::string, chrome::BleV2RemotePeripheral>
      discovered_ble_peripherals_map_;

  // Group registered advertisements with the same bluetooth service uuid.
  std::map<device::BluetoothUUID,
           std::vector<mojo::SharedRemote<bluetooth::mojom::Advertisement>>>
      registered_advertisements_map_;

  // |adapter_observer_| is only set and bound during active discovery so that
  // events we don't care about outside of discovery don't pile up.
  mojo::Receiver<bluetooth::mojom::AdapterObserver> adapter_observer_{this};

  base::WeakPtrFactory<BleV2Medium> weak_ptr_factory_{this};
};
}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_MEDIUM_H_
