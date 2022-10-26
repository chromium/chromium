// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PRESENCE_MONITOR_DELEGATE_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PRESENCE_MONITOR_DELEGATE_H_

#include "chromeos/ash/services/secure_channel/ble_scanner.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/presence_monitor.h"

namespace device {
class BluetoothAdapter;
class BluetoothDevice;
}  // namespace device

namespace ash {

namespace multidevice {
class RemoteDeviceCache;
class RemoteDeviceRef;
struct RemoteDevice;
}  // namespace multidevice

namespace secure_channel {

class BleSynchronizerBase;
class BluetoothHelper;

// Monitors device proximity while a secure channel is active.
class PresenceMonitorDelegate : public BleScanner::Observer {
 public:
  PresenceMonitorDelegate(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      PresenceMonitor::DeviceSeenCallback device_seen_callback);
  ~PresenceMonitorDelegate() override;

  PresenceMonitorDelegate(const PresenceMonitorDelegate&) = delete;
  PresenceMonitorDelegate& operator=(const PresenceMonitorDelegate&) = delete;

  void StartMonitoring(const multidevice::RemoteDevice& remote_device,
                       const multidevice::RemoteDevice& local_device);
  void StopMonitoring();

 private:
  // BleScanner::Observer:
  void OnReceivedAdvertisement(multidevice::RemoteDeviceRef remote_device,
                               device::BluetoothDevice* bluetooth_device,
                               ConnectionMedium connection_medium,
                               ConnectionRole connection_role,
                               const std::vector<uint8_t>& eid) override;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  PresenceMonitor::DeviceSeenCallback device_seen_callback_;
  std::unique_ptr<multidevice::RemoteDeviceCache> remote_device_cache_;
  std::unique_ptr<BluetoothHelper> bluetooth_helper_;
  std::unique_ptr<BleSynchronizerBase> ble_synchronizer_;
  std::unique_ptr<BleScanner> ble_scanner_;

  std::string remote_device_id_;
  std::string local_device_id_;
};

}  // namespace secure_channel
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PRESENCE_MONITOR_DELEGATE_H_
