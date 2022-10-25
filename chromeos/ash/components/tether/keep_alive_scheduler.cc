// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/keep_alive_scheduler.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/ash/components/tether/host_scan_cache.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"

namespace ash {

namespace tether {

// static
const uint32_t KeepAliveScheduler::kKeepAliveIntervalMinutes = 3;

KeepAliveScheduler::KeepAliveScheduler(
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    ActiveHost* active_host,
    HostScanCache* host_scan_cache,
    DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map)
    : KeepAliveScheduler(device_sync_client,
                         secure_channel_client,
                         active_host,
                         host_scan_cache,
                         device_id_tether_network_guid_map,
                         std::make_unique<base::RepeatingTimer>()) {}

KeepAliveScheduler::KeepAliveScheduler(
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    ActiveHost* active_host,
    HostScanCache* host_scan_cache,
    DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map,
    std::unique_ptr<base::RepeatingTimer> timer)
    : device_sync_client_(device_sync_client),
      secure_channel_client_(secure_channel_client),
      active_host_(active_host),
      host_scan_cache_(host_scan_cache),
      device_id_tether_network_guid_map_(device_id_tether_network_guid_map),
      timer_(std::move(timer)) {
  active_host_->AddObserver(this);
}

KeepAliveScheduler::~KeepAliveScheduler() {
  active_host_->RemoveObserver(this);
}

void KeepAliveScheduler::OnActiveHostChanged(
    const ActiveHost::ActiveHostChangeInfo& change_info) {
  if (change_info.new_status == ActiveHost::ActiveHostStatus::DISCONNECTED) {
    DCHECK(!change_info.new_active_host);
    DCHECK(change_info.new_wifi_network_guid.empty());

    keep_alive_operation_.reset();
    active_host_device_ = absl::nullopt;
    timer_->Stop();
    return;
  }

  if (change_info.new_status == ActiveHost::ActiveHostStatus::CONNECTED) {
    DCHECK(change_info.new_active_host);
    active_host_device_ = change_info.new_active_host;
    timer_->Start(FROM_HERE, base::Minutes(kKeepAliveIntervalMinutes),
                  base::BindRepeating(&KeepAliveScheduler::SendKeepAliveTickle,
                                      weak_ptr_factory_.GetWeakPtr()));
    SendKeepAliveTickle();
  }
}

void KeepAliveScheduler::OnOperationFinished(
    multidevice::RemoteDeviceRef remote_device,
    std::unique_ptr<DeviceStatus> device_status) {
  // Make a copy before destroying the operation below.
  const multidevice::RemoteDeviceRef device_copy = remote_device;

  keep_alive_operation_->RemoveObserver(this);
  keep_alive_operation_.reset();

  base::UmaHistogramBoolean("InstantTethering.KeepAliveTickle.Result",
                            device_status.get());

  if (!device_status) {
    // If the operation did not complete successfully, there is no new
    // information with which to update the cache.
    PA_LOG(WARNING) << "Failed to send KeepAliveTickle message.";
    return;
  }

  PA_LOG(INFO) << "Successfully sent KeepAliveTickle message.";

  std::string carrier;
  int32_t battery_percentage;
  int32_t signal_strength;
  NormalizeDeviceStatus(*device_status, &carrier, &battery_percentage,
                        &signal_strength);

  // Update the cache. Note that SetSetupRequired(false) is called because it is
  // assumed that setup is no longer required for an active connection attempt.
  host_scan_cache_->SetHostScanResult(
      *HostScanCacheEntry::Builder()
           .SetTetherNetworkGuid(
               device_id_tether_network_guid_map_
                   ->GetTetherNetworkGuidForDeviceId(device_copy.GetDeviceId()))
           .SetDeviceName(device_copy.name())
           .SetCarrier(carrier)
           .SetBatteryPercentage(battery_percentage)
           .SetSignalStrength(signal_strength)
           .SetSetupRequired(false)
           .Build());
}

void KeepAliveScheduler::SendKeepAliveTickle() {
  DCHECK(active_host_device_);

  keep_alive_operation_ = KeepAliveOperation::Factory::Create(
      *active_host_device_, device_sync_client_, secure_channel_client_);
  keep_alive_operation_->AddObserver(this);
  keep_alive_operation_->Initialize();
}

}  // namespace tether

}  // namespace ash
