// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_KEEP_ALIVE_SCHEDULER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_KEEP_ALIVE_SCHEDULER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/tether/active_host.h"
#include "chromeos/ash/components/tether/device_status_util.h"
#include "chromeos/ash/components/tether/host_connection.h"
#include "chromeos/ash/components/tether/keep_alive_operation.h"

namespace ash::tether {

class HostScanCache;
class DeviceIdTetherNetworkGuidMap;

// Schedules keep-alive messages to be sent when this device is connected to a
// remote device's tether hotspot. When a device connects, a keep-alive message
// is sent immediately, then another one is scheduled every 4 minutes until the
// device disconnects.
class KeepAliveScheduler : public ActiveHost::Observer,
                           public KeepAliveOperation::Observer {
 public:
  KeepAliveScheduler(
      raw_ptr<HostConnection::Factory> host_connection_factory,
      ActiveHost* active_host,
      HostScanCache* host_scan_cache,
      DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map);

  KeepAliveScheduler(const KeepAliveScheduler&) = delete;
  KeepAliveScheduler& operator=(const KeepAliveScheduler&) = delete;

  virtual ~KeepAliveScheduler();

  // ActiveHost::Observer:
  void OnActiveHostChanged(
      const ActiveHost::ActiveHostChangeInfo& change_info) override;

  // KeepAliveOperation::Observer:
  void OnOperationFinished(
      std::unique_ptr<DeviceStatus> device_status) override;

 private:
  friend class KeepAliveSchedulerTest;

  KeepAliveScheduler(
      raw_ptr<HostConnection::Factory> host_connection_factory,
      ActiveHost* active_host,
      HostScanCache* host_scan_cache,
      DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map,
      std::unique_ptr<base::RepeatingTimer> timer);

  void SendKeepAliveTickle();

  static const uint32_t kKeepAliveIntervalMinutes;

  raw_ptr<HostConnection::Factory> host_connection_factory_;
  raw_ptr<ActiveHost> active_host_;
  raw_ptr<HostScanCache> host_scan_cache_;
  raw_ptr<DeviceIdTetherNetworkGuidMap> device_id_tether_network_guid_map_;

  std::unique_ptr<base::RepeatingTimer> timer_;
  std::optional<multidevice::RemoteDeviceRef> active_host_device_;
  std::unique_ptr<KeepAliveOperation> keep_alive_operation_;

  base::WeakPtrFactory<KeepAliveScheduler> weak_ptr_factory_{this};
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_KEEP_ALIVE_SCHEDULER_H_
