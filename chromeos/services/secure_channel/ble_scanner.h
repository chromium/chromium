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
#include "chromeos/services/secure_channel/connection_attempt_details.h"
#include "chromeos/services/secure_channel/connection_role.h"
#include "chromeos/services/secure_channel/device_id_pair.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_medium.h"

namespace device {
class BluetoothDevice;
}  // namespace device

namespace chromeos {

namespace secure_channel {

// Performs BLE scans and notifies its delegate when an advertisement has been
// received from a remote device. This class allows clients to specify what type
// of connection they are scanning for and filters results accordingly.
class BleScanner {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnReceivedAdvertisement(
        multidevice::RemoteDeviceRef remote_device,
        device::BluetoothDevice* bluetooth_device,
        ConnectionMedium connection_medium,
        ConnectionRole connection_role) = 0;
  };

  virtual ~BleScanner();

  // Adds a scan request for the provided ConnectionAttemptDetails. If no scan
  // requests were previously present, adding a scan request will start a BLE
  // discovery session.
  void AddScanRequest(const ConnectionAttemptDetails& scan_request);

  // Removes a scan request for the provided ConnectionAttemptDetails. If this
  // function removes the only remaining request, the ongoing BLE discovery
  // session will stop.
  void RemoveScanRequest(const ConnectionAttemptDetails& scan_request);

  bool HasScanRequest(const ConnectionAttemptDetails& scan_request);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  BleScanner();

  virtual void HandleScanRequestChange() = 0;

  bool should_discovery_session_be_active() { return !scan_requests_.empty(); }
  const base::flat_set<ConnectionAttemptDetails>& scan_requests() {
    return scan_requests_;
  }
  DeviceIdPairSet GetAllDeviceIdPairs();

  void NotifyReceivedAdvertisementFromDevice(
      const multidevice::RemoteDeviceRef& remote_device,
      device::BluetoothDevice* bluetooth_device,
      ConnectionMedium connection_medium,
      ConnectionRole connection_role);

 private:
  base::ObserverList<Observer> observer_list_;

  base::flat_set<ConnectionAttemptDetails> scan_requests_;

  DISALLOW_COPY_AND_ASSIGN(BleScanner);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_SCANNER_H_
