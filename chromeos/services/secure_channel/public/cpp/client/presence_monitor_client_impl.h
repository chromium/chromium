// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_PRESENCE_MONITOR_CLIENT_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_PRESENCE_MONITOR_CLIENT_IMPL_H_

#include <memory>

#include "chromeos/services/secure_channel/public/cpp/client/presence_monitor_client.h"
#include "chromeos/services/secure_channel/public/cpp/shared/presence_monitor.h"

namespace chromeos {

namespace multidevice {

class RemoteDeviceRef;

}  // namespace multidevice

namespace secure_channel {

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

}  // namespace secure_channel
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash {
namespace secure_channel {
using ::chromeos::secure_channel::PresenceMonitorClientImpl;
}
}  // namespace ash

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_PRESENCE_MONITOR_CLIENT_IMPL_H_
