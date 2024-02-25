// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_SCANNER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_SCANNER_H_

#include <optional>
#include <ostream>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/secure_channel/connection_attempt_details.h"
#include "chromeos/ash/services/secure_channel/connection_role.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_medium.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom-shared.h"

namespace device {
class BluetoothDevice;
}

namespace ash::secure_channel {

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
        ConnectionRole connection_role,
        const std::vector<uint8_t>& eid) = 0;
    virtual void OnDiscoveryFailed(
        const DeviceIdPair& device_id_pair,
        mojom::DiscoveryResult discovery_result,
        std::optional<mojom::DiscoveryErrorCode> error_code) {}
  };

  BleScanner(const BleScanner&) = delete;
  BleScanner& operator=(const BleScanner&) = delete;

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

  // Retrieves the timestamp of the last successful discovery for the given
  // |remote_device_id|, or nullopt if we haven't seen this remote device during
  // the current Chrome session.
  std::optional<base::Time> GetLastSeenTimestamp(
      const std::string& remote_device_id);

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
      ConnectionRole connection_role,
      const std::vector<uint8_t>& eid);

  void NotifyBleDiscoverySessionFailed(
      const DeviceIdPair& device_id_pair,
      mojom::DiscoveryResult discovery_state,
      std::optional<mojom::DiscoveryErrorCode> error_code);

 private:
  base::ObserverList<Observer> observer_list_;

  base::flat_set<ConnectionAttemptDetails> scan_requests_;
  base::flat_map<std::string, base::Time>
      remote_device_id_to_last_seen_timestamp_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_SCANNER_H_
