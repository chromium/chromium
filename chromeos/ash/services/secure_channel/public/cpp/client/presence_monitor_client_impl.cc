// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/public/cpp/client/presence_monitor_client_impl.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/presence_monitor.h"

namespace ash::secure_channel {

// static
PresenceMonitorClientImpl::Factory*
    PresenceMonitorClientImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<PresenceMonitorClient>
PresenceMonitorClientImpl::Factory::Create(
    std::unique_ptr<PresenceMonitor> presence_monitor) {
  if (test_factory_) {
    return test_factory_->CreateInstance(std::move(presence_monitor));
  }

  return base::WrapUnique(
      new PresenceMonitorClientImpl(std::move(presence_monitor)));
}

// static
void PresenceMonitorClientImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

PresenceMonitorClientImpl::Factory::~Factory() = default;

PresenceMonitorClientImpl::PresenceMonitorClientImpl(
    std::unique_ptr<PresenceMonitor> presence_monitor)
    : presence_monitor_(std::move(presence_monitor)) {}

PresenceMonitorClientImpl::~PresenceMonitorClientImpl() = default;

void PresenceMonitorClientImpl::SetPresenceMonitorCallbacks(
    PresenceMonitor::ReadyCallback ready_callback,
    PresenceMonitor::DeviceSeenCallback device_seen_callback) {
  presence_monitor_->SetPresenceMonitorCallbacks(
      std::move(ready_callback), std::move(device_seen_callback));
}

void PresenceMonitorClientImpl::StartMonitoring(
    const multidevice::RemoteDeviceRef& remote_device,
    const multidevice::RemoteDeviceRef& local_device) {
  presence_monitor_->StartMonitoring(remote_device.GetRemoteDevice(),
                                     local_device.GetRemoteDevice());
}

void PresenceMonitorClientImpl::StopMonitoring() {
  presence_monitor_->StopMonitoring();
}

}  // namespace ash::secure_channel
