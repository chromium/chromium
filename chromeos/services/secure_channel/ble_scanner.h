// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_SCANNER_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_SCANNER_H_

#include <ostream>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/services/secure_channel/connection_role.h"
#include "chromeos/services/secure_channel/device_id_pair.h"

namespace device {
class BluetoothDevice;
}

namespace chromeos {

namespace secure_channel {

// Performs BLE scans and notifies its delegate when an advertisement has been
// received from a remote device.
class BleScanner {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnReceivedAdvertisement(
        multidevice::RemoteDeviceRef remote_device,
        device::BluetoothDevice* bluetooth_device,
        ConnectionRole connection_role) = 0;
  };

  virtual ~BleScanner();

  using ScanFilter = std::pair<DeviceIdPair, ConnectionRole>;

  // Adds a scan filter for the provided ScanFilter. If no scan filters were
  // previously present, adding a scan filter will start a BLE discovery session
  // and attempt to create a connection.
  void AddScanFilter(const ScanFilter& scan_filter);

  // Removes a scan filter for the provided ScanFilter. If this function
  // removes the only remaining filter, the ongoing BLE discovery session will
  // stop.
  void RemoveScanFilter(const ScanFilter& scan_filter);

  bool HasScanFilter(const ScanFilter& scan_filter);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  BleScanner();

  virtual void HandleScanFilterChange() = 0;

  bool should_discovery_session_be_active() { return !scan_filters_.empty(); }
  const base::flat_set<ScanFilter>& scan_filters() { return scan_filters_; }
  DeviceIdPairSet GetAllDeviceIdPairs();

  void NotifyReceivedAdvertisementFromDevice(
      const multidevice::RemoteDeviceRef& remote_device,
      device::BluetoothDevice* bluetooth_device,
      ConnectionRole connection_role);

 private:
  base::ObserverList<Observer> observer_list_;

  base::flat_set<ScanFilter> scan_filters_;

  DISALLOW_COPY_AND_ASSIGN(BleScanner);
};

std::ostream& operator<<(std::ostream& stream,
                         const BleScanner::ScanFilter& scan_filter);

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_SCANNER_H_
