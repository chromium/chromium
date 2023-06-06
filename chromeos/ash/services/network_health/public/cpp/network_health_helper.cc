// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/network_health/public/cpp/network_health_helper.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/ash/services/network_health/in_process_instance.h"
#include "chromeos/ash/services/network_health/network_health_service.h"
#include "url/gurl.h"

namespace ash::network_health {

namespace mojom = ::chromeos::network_health::mojom;

// static
std::unique_ptr<NetworkHealthHelper> NetworkHealthHelper::CreateForTesting(
    NetworkHealthService* network_health_service) {
  // Use WrapUnique for private constructor.
  return base::WrapUnique(new NetworkHealthHelper(network_health_service));
}

NetworkHealthHelper::NetworkHealthHelper(
    NetworkHealthService* network_health_service) {
  SetupRemote(network_health_service);
  RequestNetworks();
}

NetworkHealthHelper::NetworkHealthHelper() {
  RequestNetworks();
}

NetworkHealthHelper::~NetworkHealthHelper() = default;

void NetworkHealthHelper::OnConnectionStateChanged(const std::string& guid,
                                                   mojom::NetworkState state) {
  if (default_network_ && default_network_->guid == guid) {
    default_network_->state = state;
  }
}

void NetworkHealthHelper::OnSignalStrengthChanged(
    const std::string& /*guid*/,
    mojom::UInt32ValuePtr /*signal_strength*/) {}

void NetworkHealthHelper::OnNetworkListChanged(
    std::vector<mojom::NetworkPtr> networks) {
  NetworkListReceived(std::move(networks));
}

mojom::NetworkHealthService* NetworkHealthHelper::GetNetworkHealthService() {
  if (!remote_) {
    SetupRemote(GetInProcessInstance());
  }
  return remote_.get();
}

void NetworkHealthHelper::SetupRemote(
    NetworkHealthService* network_health_service) {
  auto receiver = remote_.BindNewPipeAndPassReceiver();
  network_health_service->BindReceiver(std::move(receiver));
  if (!observer_receiver_.is_bound()) {
    remote_->AddObserver(observer_receiver_.BindNewPipeAndPassRemote());
    observer_receiver_.set_disconnect_handler(base::BindOnce(
        &NetworkHealthHelper::OnMojoError, base::Unretained(this)));
  }
}

void NetworkHealthHelper::RequestNetworks() {
  auto* remote = GetNetworkHealthService();
  if (!remote) {
    NetworkListReceived({});
    return;
  }
  remote->GetNetworkList(base::BindOnce(
      &NetworkHealthHelper::NetworkListReceived, base::Unretained(this)));
}

chromeos::network_config::mojom::PortalState
NetworkHealthHelper::WiFiPortalState() {
  using PortalState = chromeos::network_config::mojom::PortalState;
  if (!default_network_ ||
      default_network_->type !=
          chromeos::network_config::mojom::NetworkType::kWiFi) {
    return PortalState::kUnknown;
  }
  return default_network_->portal_state;
}

void NetworkHealthHelper::NetworkListReceived(
    std::vector<mojom::NetworkPtr> networks) {
  if (networks.size()) {
    default_network_ = networks[0].Clone();
  } else {
    default_network_ = nullptr;
  }
}

void NetworkHealthHelper::OnMojoError() {
  remote_.reset();
  observer_receiver_.reset();
}

}  // namespace ash::network_health
