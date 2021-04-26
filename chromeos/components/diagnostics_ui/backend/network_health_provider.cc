// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/network_health_provider.h"

#include <string>
#include <utility>

#include "chromeos/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"

namespace chromeos {
namespace diagnostics {
namespace {

namespace network_mojom = chromeos::network_config::mojom;
using network_mojom::NetworkType;

bool IsSupportedNetworkType(network_mojom::NetworkType type) {
  switch (type) {
    case NetworkType::kWiFi:
    case NetworkType::kCellular:
    case NetworkType::kEthernet:
      return true;
    case NetworkType::kMobile:
    case NetworkType::kTether:
    case NetworkType::kVPN:
    case NetworkType::kAll:
    case NetworkType::kWireless:
      return false;
  }
}

}  // namespace

NetworkHealthProvider::NetworkHealthProvider() {
  network_config::BindToInProcessInstance(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  remote_cros_network_config_->AddObserver(
      cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());
}

NetworkHealthProvider::~NetworkHealthProvider() = default;

void NetworkHealthProvider::OnNetworkStateListChanged() {}

void NetworkHealthProvider::OnDeviceStateListChanged() {
  remote_cros_network_config_->GetDeviceStateList(
      base::BindOnce(&NetworkHealthProvider::OnDeviceStateListReceived,
                     base::Unretained(this)));
}

void NetworkHealthProvider::OnActiveNetworksChanged(
    std::vector<network_mojom::NetworkStatePropertiesPtr> active_networks) {
  OnActiveNetworkStateListReceived(std::move(active_networks));
}

void NetworkHealthProvider::OnNetworkStateChanged(
    network_mojom::NetworkStatePropertiesPtr network_state) {}
void NetworkHealthProvider::OnVpnProvidersChanged() {}
void NetworkHealthProvider::OnNetworkCertificatesChanged() {}

void NetworkHealthProvider::OnActiveNetworkStateListReceived(
    std::vector<network_mojom::NetworkStatePropertiesPtr> networks) {
  guid_to_network_map_.clear();
  for (auto& network : networks) {
    if (IsSupportedNetworkType(network->type)) {
      guid_to_network_map_[network->guid] = std::move(network);
    }
  }
  // TODO(michaelcheco): Call Mojo API here.
}

void NetworkHealthProvider::OnDeviceStateListReceived(
    std::vector<network_mojom::DeviceStatePropertiesPtr> devices) {
  device_type_map_.clear();
  for (auto& device : devices) {
    if (IsSupportedNetworkType(device->type)) {
      device_type_map_.emplace(device->type, std::move(device));
    }
  }
}

std::vector<std::string> NetworkHealthProvider::GetNetworkGuidListForTesting() {
  std::vector<std::string> network_guids;
  for (const auto& entry : guid_to_network_map_) {
    network_guids.push_back(entry.first);
  }
  return network_guids;
}

const DeviceMap& NetworkHealthProvider::GetDeviceTypeMapForTesting() {
  return device_type_map_;
}

}  // namespace diagnostics
}  // namespace chromeos
