// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PRESENCE_MONITOR_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PRESENCE_MONITOR_IMPL_H_

#include "chromeos/services/secure_channel/presence_monitor_delegate.h"
#include "chromeos/services/secure_channel/public/cpp/shared/presence_monitor.h"

namespace device {

class BluetoothAdapter;

}  // namespace device

namespace chromeos {

namespace multidevice {

struct RemoteDevice;

}  // namespace multidevice

namespace secure_channel {

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

}  // namespace secure_channel
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash {
namespace secure_channel {
using ::chromeos::secure_channel::PresenceMonitorImpl;
}
}  // namespace ash

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PRESENCE_MONITOR_IMPL_H_
