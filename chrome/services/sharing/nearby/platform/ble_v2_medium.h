// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_MEDIUM_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_MEDIUM_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/services/sharing/nearby/platform/ble_v2_peripheral.h"
#include "third_party/nearby/src/internal/platform/implementation/ble_v2.h"

namespace nearby::chrome {

// Concrete ble_v2::BleMedium implementation.
// Productionized impl will also consumes bluetooth::mojom::Adapter methods.
// Current skeleton has not added it yet.
class BleV2Medium : public ::nearby::api::ble_v2::BleMedium {
 public:
  BleV2Medium();
  ~BleV2Medium() override;

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

 private:
  // This will eventually be removed once the class is productionized.
  void SimulateAdvertisementFound(BleV2Medium::ScanningCallback callback);

  // Keyed by address of advertising remote device. Note: References to these
  // BlePeripherals are passed to Nearby Connections. This is safe because, for
  // std::map, insert/emplace do not invalidate references, and the erase
  // operation only invalidates the reference to the erased element.
  std::map<std::string, chrome::BleV2Peripheral>
      discovered_ble_peripherals_map_;

  base::WeakPtrFactory<BleV2Medium> weak_ptr_factory_{this};
};
}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_MEDIUM_H_
