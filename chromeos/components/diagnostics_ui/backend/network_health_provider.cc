// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/network_health_provider.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
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

NetworkProperties::NetworkProperties(
    network_mojom::NetworkStatePropertiesPtr network_state)
    : network_state(std::move(network_state)) {}

NetworkProperties::~NetworkProperties() = default;

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
  network_properties_map_.clear();
  for (auto& network : networks) {
    if (IsSupportedNetworkType(network->type)) {
      const std::string guid = mojo::Clone(network->guid);
      network_properties_map_.emplace(guid, std::move(network));
      // This method depends on the |network_properties_map_| being populated
      // before being called.
      GetManagedPropertiesForNetwork(guid);
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
  network_guids.reserve(network_properties_map_.size());
  for (const auto& entry : network_properties_map_) {
    network_guids.push_back(entry.first);
  }
  return network_guids;
}

const DeviceMap& NetworkHealthProvider::GetDeviceTypeMapForTesting() {
  return device_type_map_;
}

const NetworkPropertiesMap&
NetworkHealthProvider::GetNetworkPropertiesMapForTesting() {
  return network_properties_map_;
}

void NetworkHealthProvider::GetManagedPropertiesForNetwork(
    const std::string& guid) {
  remote_cros_network_config_->GetManagedProperties(
      guid, base::BindOnce(&NetworkHealthProvider::OnManagedPropertiesReceived,
                           base::Unretained(this), guid));
}

void NetworkHealthProvider::OnManagedPropertiesReceived(
    const std::string& guid,
    network_mojom::ManagedPropertiesPtr managed_properties) {
  if (!managed_properties) {
    DVLOG(1) << "No managed properties found for guid: " << guid;
    return;
  }
  // Add managed properties to corresponding NetworkProperties struct.
  DCHECK(base::Contains(network_properties_map_, guid));
  auto network_props_iter = network_properties_map_.find(guid);
  network_props_iter->second.managed_properties = std::move(managed_properties);
}

}  // namespace diagnostics
}  // namespace chromeos
