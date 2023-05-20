// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_MEDIUM_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_MEDIUM_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/services/sharing/nearby/platform/ble_v2_peripheral.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/nearby/src/internal/platform/implementation/ble_v2.h"

namespace nearby::chrome {

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
  chrome::BleV2Peripheral* GetDiscoveredBlePeripheral(
      const std::string& address);
  Uuid BluetoothServiceUuidToNearbyUuid(
      const device::BluetoothUUID& bluetooth_service_uuid);

  mojo::SharedRemote<bluetooth::mojom::Adapter> adapter_;

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
  std::map<std::string, chrome::BleV2Peripheral>
      discovered_ble_peripherals_map_;

  // |adapter_observer_| is only set and bound during active discovery so that
  // events we don't care about outside of discovery don't pile up.
  mojo::Receiver<bluetooth::mojom::AdapterObserver> adapter_observer_{this};

  base::WeakPtrFactory<BleV2Medium> weak_ptr_factory_{this};
};
}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_MEDIUM_H_
