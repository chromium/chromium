// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/interactive/cellular/wait_for_service_connected_observer.h"

#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"

namespace ash {

WaitForServiceConnectedObserver::WaitForServiceConnectedObserver(
    NetworkStateHandler* network_state_handler,
    const std::string& iccid)
    : ObservationStateObserver(network_state_handler), iccid_(iccid) {}

WaitForServiceConnectedObserver::~WaitForServiceConnectedObserver() = default;

void WaitForServiceConnectedObserver::NetworkPropertiesUpdated(
    const NetworkState* network) {
  // Only mark the network as connected if the ICCID matches, the network is
  // connectable, and the network isn't already connected.
  if (network->iccid() != iccid_ || !network->connectable() ||
      network->IsConnectedState()) {
    return;
  }
  ShillServiceClient::Get()->GetTestInterface()->SetServiceProperty(
      network->path(), shill::kStateProperty, base::Value(shill::kStateOnline));
}

void WaitForServiceConnectedObserver::NetworkConnectionStateChanged(
    const NetworkState*) {
  OnStateObserverStateChanged(/*state=*/IsServiceConnected());
}

bool WaitForServiceConnectedObserver::GetStateObserverInitialState() const {
  return IsServiceConnected();
}

bool WaitForServiceConnectedObserver::IsServiceConnected() const {
  NetworkStateHandler::NetworkStateList network_state_list;
  NetworkHandler::Get()->network_state_handler()->GetVisibleNetworkListByType(
      NetworkTypePattern::Cellular(), &network_state_list);
  for (const NetworkState* network : network_state_list) {
    if (network->iccid() == iccid_) {
      return network->IsConnectedState();
    }
  }
  return false;
}

}  // namespace ash
