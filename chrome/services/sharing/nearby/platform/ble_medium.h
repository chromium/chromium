// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_MEDIUM_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_MEDIUM_H_

#include <string>

#include "chrome/services/sharing/nearby/platform/ble_peripheral.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/nearby/src/internal/platform/implementation/ble.h"

namespace nearby::chrome {

// Concrete BleMedium implementation.
// api::BleMedium is a synchronous interface, so this implementation consumes
// the synchronous signatures of bluetooth::mojom::Adapter methods.
class BleMedium : public api::BleMedium,
                  public bluetooth::mojom::AdapterObserver {
 public:
  explicit BleMedium(
      const mojo::SharedRemote<bluetooth::mojom::Adapter>& adapter);
  ~BleMedium() override;

  BleMedium(const BleMedium&) = delete;
  BleMedium& operator=(const BleMedium&) = delete;

  // api::BleMedium:
  bool StartAdvertising(
      const std::string& service_id,
      const ByteArray& advertisement,
      const std::string& fast_advertisement_service_uuid) override;
  bool StopAdvertising(const std::string& service_id) override;
  bool StartScanning(
      const std::string& service_id,
      const std::string& fast_advertisement_service_uuid,
      DiscoveredPeripheralCallback discovered_peripheral_callback) override;
  bool StopScanning(const std::string& service_id) override;
  bool StartAcceptingConnections(
      const std::string& service_id,
      AcceptedConnectionCallback accepted_connection_callback) override;
  bool StopAcceptingConnections(const std::string& service_id) override;
  std::unique_ptr<api::BleSocket> Connect(
      api::BlePeripheral& ble_peripheral,
      const std::string& service_id,
      CancellationFlag* cancellation_flag) override;

 private:
  // bluetooth::mojom::AdapterObserver:
  void PresentChanged(bool present) override;
  void PoweredChanged(bool powered) override;
  void DiscoverableChanged(bool discoverable) override;
  void DiscoveringChanged(bool discovering) override;
  void DeviceAdded(bluetooth::mojom::DeviceInfoPtr device) override;
  void DeviceChanged(bluetooth::mojom::DeviceInfoPtr device) override;
  void DeviceRemoved(bluetooth::mojom::DeviceInfoPtr device) override;

  // A bluetooth::mojom::Advertisement message pipe was destroyed.
  void AdvertisementReleased(const device::BluetoothUUID& service_uuid);

  // Query if any service IDs are being scanned for.
  bool IsScanning();

  // End discovery for all requested services.
  void StopScanning();

  // Returns nullptr if no BlePeripheral at |address| exists.
  chrome::BlePeripheral* GetDiscoveredBlePeripheral(const std::string& address);

  mojo::SharedRemote<bluetooth::mojom::Adapter> adapter_;

  // |adapter_observer_| is only set and bound during active discovery so that
  // events we don't care about outside of discovery don't pile up.
  mojo::Receiver<bluetooth::mojom::AdapterObserver> adapter_observer_{this};

  // A map of service IDs (e.g., "NearbySharing") to their respective fast
  // advertisement UUIDs, as informed by active registered advertisements.
  std::map<std::string, device::BluetoothUUID>
      registered_service_id_to_fast_advertisement_service_uuid_map_;

  // Keyed by service UUID of the advertisement.
  std::map<device::BluetoothUUID, mojo::Remote<bluetooth::mojom::Advertisement>>
      registered_advertisements_map_;

  // A map of service IDs (e.g., "NearbySharing") to their respective fast
  // advertisement UUIDs used to filter incoming BLE advertisements during
  // scanning. We assume that the mapping is one-to-one. Insertions and
  // deletions mirror those of |discovered_peripheral_callbacks_map_|.
  std::map<std::string, device::BluetoothUUID>
      discovery_service_id_to_fast_advertisement_service_uuid_map_;

  // Keyed by requested service UUID. Discovery is active as long as this map is
  // non-empty.
  std::map<device::BluetoothUUID, DiscoveredPeripheralCallback>
      discovered_peripheral_callbacks_map_;

  // Only set while discovery is active.
  mojo::Remote<bluetooth::mojom::DiscoverySession> discovery_session_;

  // Keyed by address of advertising remote device. Note: References to these
  // BlePeripherals are passed to Nearby Connections. This is safe because, for
  // std::map, insert/emplace do not invalidate references, and the erase
  // operation only invalidates the reference to the erased element.
  std::map<std::string, chrome::BlePeripheral> discovered_ble_peripherals_map_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_MEDIUM_H_
