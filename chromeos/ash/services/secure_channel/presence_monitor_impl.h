// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PRESENCE_MONITOR_IMPL_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PRESENCE_MONITOR_IMPL_H_

#include "chromeos/ash/services/secure_channel/public/cpp/shared/presence_monitor.h"

namespace device {
class BluetoothAdapter;
}

namespace ash::secure_channel {

class PresenceMonitorDelegate;

// Monitors device proximity while a secure channel is active.
class PresenceMonitorImpl : public PresenceMonitor {
 public:
  PresenceMonitorImpl();
  ~PresenceMonitorImpl() override;

  PresenceMonitorImpl(const PresenceMonitorImpl&) = delete;
  PresenceMonitorImpl& operator=(const PresenceMonitorImpl&) = delete;

  // PresenceMonitor:
  void SetPresenceMonitorCallbacks(
      PresenceMonitor::ReadyCallback ready_callback,
      PresenceMonitor::DeviceSeenCallback device_seen_callback) override;
  void StartMonitoring(const multidevice::RemoteDevice& remote_device,
                       const multidevice::RemoteDevice& local_device) override;
  void StopMonitoring() override;

 private:
  void OnAdapterReceived(
      PresenceMonitor::ReadyCallback ready_callback,
      PresenceMonitor::DeviceSeenCallback device_seen_callback,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);

  std::unique_ptr<PresenceMonitorDelegate> presence_monitor_delegate_;

  base::WeakPtrFactory<PresenceMonitorImpl> weak_ptr_factory_{this};
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PRESENCE_MONITOR_IMPL_H_
