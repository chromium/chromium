// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/active_host_network_state_updater.h"

#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/tether/active_host.h"

namespace ash {

namespace tether {

ActiveHostNetworkStateUpdater::ActiveHostNetworkStateUpdater(
    ActiveHost* active_host,
    NetworkStateHandler* network_state_handler)
    : active_host_(active_host), network_state_handler_(network_state_handler) {
  active_host_->AddObserver(this);
}

ActiveHostNetworkStateUpdater::~ActiveHostNetworkStateUpdater() {
  active_host_->RemoveObserver(this);
}

void ActiveHostNetworkStateUpdater::OnActiveHostChanged(
    const ActiveHost::ActiveHostChangeInfo& change_info) {
  switch (change_info.new_status) {
    case ActiveHost::ActiveHostStatus::DISCONNECTED: {
      DCHECK(!change_info.old_active_host_id.empty());
      DCHECK(!change_info.old_tether_network_guid.empty());
      DCHECK(change_info.old_status == ActiveHost::ActiveHostStatus::CONNECTING
                 ? change_info.old_wifi_network_guid.empty()
                 : !change_info.old_wifi_network_guid.empty());

      PA_LOG(INFO) << "Active host: Disconnected from active host with ID "
                   << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                          change_info.old_active_host_id)
                   << ". Old tether network GUID: "
                   << change_info.old_tether_network_guid
                   << ", old Wi-Fi network GUID: "
                   << (change_info.old_status ==
                               ActiveHost::ActiveHostStatus::CONNECTING
                           ? "<never set>"
                           : change_info.old_wifi_network_guid);

      network_state_handler_->SetTetherNetworkStateDisconnected(
          change_info.old_tether_network_guid);
      break;
    }
    case ActiveHost::ActiveHostStatus::CONNECTING: {
      DCHECK(change_info.new_active_host);
      DCHECK(!change_info.new_tether_network_guid.empty());
      DCHECK(change_info.new_wifi_network_guid.empty());

      PA_LOG(INFO) << "Active host: Started connecting to device with ID "
                   << change_info.new_active_host->GetTruncatedDeviceIdForLogs()
                   << ". Tether network GUID: "
                   << change_info.new_tether_network_guid;

      network_state_handler_->SetTetherNetworkStateConnecting(
          change_info.new_tether_network_guid);
      break;
    }
    case ActiveHost::ActiveHostStatus::CONNECTED: {
      DCHECK(change_info.new_active_host);
      DCHECK(!change_info.new_tether_network_guid.empty());
      DCHECK(!change_info.new_wifi_network_guid.empty());

      PA_LOG(INFO) << "Active host: Connected successfully to device with ID "
                   << change_info.new_active_host->GetTruncatedDeviceIdForLogs()
                   << ". Tether network GUID: "
                   << change_info.new_tether_network_guid
                   << ", Wi-Fi network GUID: "
                   << change_info.new_wifi_network_guid;

      network_state_handler_->SetTetherNetworkStateConnected(
          change_info.new_tether_network_guid);
      break;
    }
  }
}

}  // namespace tether

}  // namespace ash
