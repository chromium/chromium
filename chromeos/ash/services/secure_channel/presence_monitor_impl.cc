// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/presence_monitor_impl.h"

#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/secure_channel/presence_monitor_delegate.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace ash::secure_channel {

PresenceMonitorImpl::PresenceMonitorImpl() = default;

PresenceMonitorImpl::~PresenceMonitorImpl() = default;

void PresenceMonitorImpl::SetPresenceMonitorCallbacks(
    ReadyCallback ready_callback,
    DeviceSeenCallback device_seen_callback) {
  if (!presence_monitor_delegate_) {
    device::BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
        &PresenceMonitorImpl::OnAdapterReceived, weak_ptr_factory_.GetWeakPtr(),
        std::move(ready_callback), std::move(device_seen_callback)));
  }
}

void PresenceMonitorImpl::StartMonitoring(
    const multidevice::RemoteDevice& remote_device,
    const multidevice::RemoteDevice& local_device) {
  if (presence_monitor_delegate_) {
    presence_monitor_delegate_->StartMonitoring(remote_device, local_device);
  }
}

void PresenceMonitorImpl::StopMonitoring() {
  if (presence_monitor_delegate_) {
    presence_monitor_delegate_->StopMonitoring();
  }
}

void PresenceMonitorImpl::OnAdapterReceived(
    PresenceMonitor::ReadyCallback ready_callback,
    PresenceMonitor::DeviceSeenCallback device_seen_callback,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  presence_monitor_delegate_ = std::make_unique<PresenceMonitorDelegate>(
      std::move(bluetooth_adapter), std::move(device_seen_callback));
  ready_callback.Run();
}

}  // namespace ash::secure_channel
