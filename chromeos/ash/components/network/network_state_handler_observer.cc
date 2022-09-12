// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace ash {

NetworkStateHandlerObserver::NetworkStateHandlerObserver() = default;

NetworkStateHandlerObserver::~NetworkStateHandlerObserver() = default;

void NetworkStateHandlerObserver::NetworkListChanged() {}

void NetworkStateHandlerObserver::DeviceListChanged() {}

void NetworkStateHandlerObserver::DefaultNetworkChanged(
    const NetworkState* network) {}

void NetworkStateHandlerObserver::PortalStateChanged(
    const NetworkState* default_network,
    NetworkState::PortalState portal_state) {}

void NetworkStateHandlerObserver::NetworkConnectionStateChanged(
    const NetworkState* network) {}

void NetworkStateHandlerObserver::ActiveNetworksChanged(
    const std::vector<const NetworkState*>& active_networks) {}

void NetworkStateHandlerObserver::NetworkPropertiesUpdated(
    const NetworkState* network) {}

void NetworkStateHandlerObserver::DevicePropertiesUpdated(
    const DeviceState* device) {}

void NetworkStateHandlerObserver::ScanRequested(
    const NetworkTypePattern& type) {}

void NetworkStateHandlerObserver::ScanStarted(const DeviceState* device) {}

void NetworkStateHandlerObserver::ScanCompleted(const DeviceState* device) {}

void NetworkStateHandlerObserver::NetworkIdentifierTransitioned(
    const std::string& old_service_path,
    const std::string& new_service_path,
    const std::string& old_guid,
    const std::string& new_guid) {}

void NetworkStateHandlerObserver::HostnameChanged(const std::string& hostname) {
}

void NetworkStateHandlerObserver::OnShuttingDown() {}

}  // namespace ash
