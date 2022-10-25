// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_PRESENCE_MONITOR_CLIENT_IMPL_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_PRESENCE_MONITOR_CLIENT_IMPL_H_

#include <memory>

#include "chromeos/ash/services/secure_channel/public/cpp/client/presence_monitor_client.h"

namespace ash::secure_channel {

class PresenceMonitor;

// Provides clients access to the PresenceMonitor API.
class PresenceMonitorClientImpl : public PresenceMonitorClient {
 public:
  class Factory {
   public:
    static std::unique_ptr<PresenceMonitorClient> Create(
        std::unique_ptr<PresenceMonitor> presence_monitor);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<PresenceMonitorClient> CreateInstance(
        std::unique_ptr<PresenceMonitor> presence_monitor) = 0;

   private:
    static Factory* test_factory_;
  };

  ~PresenceMonitorClientImpl() override;

 private:
  explicit PresenceMonitorClientImpl(
      std::unique_ptr<PresenceMonitor> presence_monitor);

  // PresenceMonitorClient:
  void SetPresenceMonitorCallbacks(
      PresenceMonitor::ReadyCallback ready_callback,
      PresenceMonitor::DeviceSeenCallback device_seen_callback) override;
  void StartMonitoring(
      const multidevice::RemoteDeviceRef& remote_device_ref,
      const multidevice::RemoteDeviceRef& local_device_ref) override;
  void StopMonitoring() override;

  std::unique_ptr<PresenceMonitor> presence_monitor_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_PRESENCE_MONITOR_CLIENT_IMPL_H_
