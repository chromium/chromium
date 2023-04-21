// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/notification_remover.h"

#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/tether/notification_presenter.h"

namespace ash::tether {

NotificationRemover::NotificationRemover(
    NetworkStateHandler* network_state_handler,
    NotificationPresenter* notification_presenter,
    HostScanCache* host_scan_cache,
    ActiveHost* active_host)
    : network_state_handler_(network_state_handler),
      notification_presenter_(notification_presenter),
      host_scan_cache_(host_scan_cache),
      active_host_(active_host) {
  network_state_handler_observer_.Observe(network_state_handler_.get());
  host_scan_cache_->AddObserver(this);
  active_host_->AddObserver(this);
}

NotificationRemover::~NotificationRemover() {
  active_host_->RemoveObserver(this);
  host_scan_cache_->RemoveObserver(this);

  // When the Tether component is shut down, "Available Hotspot", "Setup
  // Required", and "Connection Failed" notifications should be removed. The
  // "Enable Bluetooth" notification should not be removed, because it is
  // informative when Tether is disabled due to Bluetooth being disabled.
  notification_presenter_->RemovePotentialHotspotNotification();
  notification_presenter_->RemoveSetupRequiredNotification();
  notification_presenter_->RemoveConnectionToHostFailedNotification();
}

void NotificationRemover::OnCacheBecameEmpty() {
  notification_presenter_->RemovePotentialHotspotNotification();
}

void NotificationRemover::NetworkConnectionStateChanged(
    const NetworkState* network) {
  // Note: If a network is active (i.e., connecting or connected), it will be
  // returned at the front of the list, so using FirstNetworkByType() guarantees
  // that we will find an active network if there is one.
  const NetworkState* first_network =
      network_state_handler_->FirstNetworkByType(NetworkTypePattern::Default());
  if (first_network && first_network->IsConnectingOrConnected())
    notification_presenter_->RemovePotentialHotspotNotification();
}

void NotificationRemover::OnShuttingDown() {
  network_state_handler_observer_.Reset();
}

void NotificationRemover::OnActiveHostChanged(
    const ActiveHost::ActiveHostChangeInfo& active_host_change_info) {
  if (active_host_change_info.new_status !=
      ActiveHost::ActiveHostStatus::DISCONNECTED)
    notification_presenter_->RemovePotentialHotspotNotification();
}

}  // namespace ash::tether
