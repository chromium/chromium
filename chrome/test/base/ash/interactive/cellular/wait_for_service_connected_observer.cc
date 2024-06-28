// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/interactive/cellular/wait_for_service_connected_observer.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "dbus/object_path.h"

namespace ash {

WaitForServiceConnectedObserver::WaitForServiceConnectedObserver(
    const std::string& iccid)
    : ObservationStateObserver(NetworkHandler::Get()->network_state_handler()),
      iccid_(iccid) {}

WaitForServiceConnectedObserver::~WaitForServiceConnectedObserver() = default;

void WaitForServiceConnectedObserver::NetworkPropertiesUpdated(
    const NetworkState* network) {
  // Only mark the network as connected if the ICCID matches, the network is
  // connectable, and the network isn't already connected.
  if (network->iccid() != iccid_ || !network->connectable() ||
      network->IsConnectedState()) {
    return;
  }
  ShillServiceClient::Get()->Connect(dbus::ObjectPath(network->path()),
                                     /*callback=*/base::DoNothing(),
                                     /*error_callback=*/base::DoNothing());
}

void WaitForServiceConnectedObserver::NetworkConnectionStateChanged(
    const NetworkState* network) {
  if (network->iccid() == iccid_) {
    OnStateObserverStateChanged(/*state=*/IsServiceConnected());
  }
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
