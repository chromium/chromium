// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/network_health/network_health_service.h"

#include <cstdint>
#include <cstdlib>
#include <map>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"
#include "chromeos/ash/services/network_health/network_health_constants.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health_types.mojom.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace ash::network_health {

namespace {

namespace mojom = ::chromeos::network_health::mojom;
namespace network_config = ::chromeos::network_config;

constexpr mojom::NetworkState DeviceStateToNetworkState(
    network_config::mojom::DeviceStateType device_state) {
  switch (device_state) {
    case network_config::mojom::DeviceStateType::kUninitialized:
      return mojom::NetworkState::kUninitialized;
    case network_config::mojom::DeviceStateType::kDisabled:
    case network_config::mojom::DeviceStateType::kDisabling:
    case network_config::mojom::DeviceStateType::kEnabling:
      // Disabling and Enabling are intermediate state that we care about in the
      // UI, but not for purposes of network health, we can treat as Disabled.
      return mojom::NetworkState::kDisabled;
    case network_config::mojom::DeviceStateType::kEnabled:
      return mojom::NetworkState::kNotConnected;
    case network_config::mojom::DeviceStateType::kProhibited:
      return mojom::NetworkState::kProhibited;
    case network_config::mojom::DeviceStateType::kUnavailable:
      NOTREACHED_IN_MIGRATION();
      return mojom::NetworkState::kUninitialized;
  }
}

constexpr mojom::NetworkState ConnectionStateToNetworkState(
    network_config::mojom::ConnectionStateType connection_state) {
  switch (connection_state) {
    case network_config::mojom::ConnectionStateType::kOnline:
      return mojom::NetworkState::kOnline;
    case network_config::mojom::ConnectionStateType::kConnected:
      return mojom::NetworkState::kConnected;
    case network_config::mojom::ConnectionStateType::kPortal:
      return mojom::NetworkState::kPortal;
    case network_config::mojom::ConnectionStateType::kConnecting:
      return mojom::NetworkState::kConnecting;
    case network_config::mojom::ConnectionStateType::kNotConnected:
      return mojom::NetworkState::kNotConnected;
  }
}

// Populates a mojom::NetworkPtr based on the given |device_prop| and
// |network_prop|. This function assumes that |device_prop| is populated, while
// |network_prop| could be null.
mojom::NetworkPtr CreateNetwork(
    const network_config::mojom::DeviceStatePropertiesPtr& device_prop,
    const network_config::mojom::NetworkStatePropertiesPtr& net_prop) {
  auto net = mojom::Network::New();
  net->mac_address = device_prop->mac_address;
  net->type = device_prop->type;
  if (device_prop->ipv6_address)
    net->ipv6_addresses.push_back(device_prop->ipv6_address->ToString());
  if (device_prop->ipv4_address)
    net->ipv4_address = device_prop->ipv4_address->ToString();

  if (net_prop) {
    net->state = ConnectionStateToNetworkState(net_prop->connection_state);
    net->name = net_prop->name;
    net->guid = net_prop->guid;
    net->portal_state = net_prop->portal_state;
    net->portal_probe_url = net_prop->portal_probe_url;
    if (network_config::NetworkTypeMatchesType(
            net_prop->type, network_config::mojom::NetworkType::kWireless)) {
      net->signal_strength = mojom::UInt32Value::New(
          network_config::GetWirelessSignalStrength(net_prop.get()));
    }
  } else {
    net->state = DeviceStateToNetworkState(device_prop->device_state);
  }

  return net;
}

// Compares everything except strength properties which are observed only on
// a per-network basis.
bool NetworksMatch(const mojom::NetworkPtr& a, const mojom::NetworkPtr& b) {
  return a->state == b->state && a->guid == b->guid && a->name == b->name &&
         a->mac_address == b->mac_address &&
         a->ipv4_address == b->ipv4_address &&
         a->ipv6_addresses == b->ipv6_addresses &&
         a->portal_state == b->portal_state &&
         a->portal_probe_url == b->portal_probe_url;
}

bool NetworkListsMatch(const std::vector<mojom::NetworkPtr>& networks_a,
                       const std::vector<mojom::NetworkPtr>& networks_b) {
  if (networks_a.size() != networks_b.size())
    return false;
  for (std::size_t i = 0u; i < networks_a.size(); ++i) {
    if (!NetworksMatch(networks_a[i], networks_b[i]))
      return false;
  }
  return true;
}

}  // namespace

NetworkHealthService::NetworkHealthService() {
  ash::network_config::BindToInProcessInstance(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  remote_cros_network_config_->AddObserver(
      cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());
  RefreshNetworkHealthState();
  SetTimer(std::make_unique<base::RepeatingTimer>());
  tracked_guids_timer_.Start(FROM_HERE, kUpdateTrackedGuidsInterval, this,
                             &NetworkHealthService::UpdateTrackedGuids);
  if (mojo_service_manager::IsServiceManagerBound()) {
    mojo_service_manager::GetServiceManagerProxy()->Register(
        chromeos::mojo_services::kChromiumNetworkHealth,
        provider_receiver_.BindNewPipeAndPassRemote());
  }
}

void NetworkHealthService::SetTimer(
    std::unique_ptr<base::RepeatingTimer> timer) {
  timer_ = std::move(timer);
  timer_->Start(FROM_HERE, kSignalStrengthSampleRate, this,
                &NetworkHealthService::AnalyzeSignalStrength);
}

NetworkHealthService::~NetworkHealthService() = default;

void NetworkHealthService::BindReceiver(
    mojo::PendingReceiver<mojom::NetworkHealthService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void NetworkHealthService::Request(
    chromeos::mojo_service_manager::mojom::ProcessIdentityPtr identity,
    mojo::ScopedMessagePipeHandle receiver) {
  BindReceiver(
      mojo::PendingReceiver<mojom::NetworkHealthService>(std::move(receiver)));
}

const mojom::NetworkHealthState& NetworkHealthService::GetNetworkHealthState() {
  NET_LOG(EVENT) << "Network Health State Requested";
  return network_health_state_;
}

const std::map<std::string, base::Time>&
NetworkHealthService::GetTrackedGuidsForTest() {
  return guid_to_active_time_;
}

void NetworkHealthService::AddObserver(
    mojo::PendingRemote<mojom::NetworkEventsObserver> observer) {
  observers_.Add(std::move(observer));
}

void NetworkHealthService::GetNetworkList(GetNetworkListCallback callback) {
  std::move(callback).Run(mojo::Clone(network_health_state_.networks));
}

void NetworkHealthService::GetHealthSnapshot(
    GetHealthSnapshotCallback callback) {
  std::move(callback).Run(network_health_state_.Clone());
}

void NetworkHealthService::GetRecentlyActiveNetworks(
    GetRecentlyActiveNetworksCallback callback) {
  std::vector<std::string> networks;
  for (auto const& [guid, timestamp] : guid_to_active_time_) {
    networks.push_back(guid);
  }
  std::move(callback).Run(std::move(networks));
}

void NetworkHealthService::OnNetworkStateListChanged() {
  RequestNetworkStateList();
}

void NetworkHealthService::OnDeviceStateListChanged() {
  RequestDeviceStateList();
}

void NetworkHealthService::OnActiveNetworksChanged(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr>
        active_networks) {
  HandleNetworkEventsForActiveNetworks(std::move(active_networks));
  RequestNetworkStateList();
}

void NetworkHealthService::OnNetworkStateChanged(
    network_config::mojom::NetworkStatePropertiesPtr network_state) {
  if (!network_state) {
    return;
  }
  HandleNetworkEventsForInactiveNetworks(std::move(network_state));
  RequestNetworkStateList();
}

void NetworkHealthService::OnNetworkStateListReceived(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> props) {
  network_properties_.swap(props);
  CreateNetworkHealthState();
}

void NetworkHealthService::OnDeviceStateListReceived(
    std::vector<network_config::mojom::DeviceStatePropertiesPtr> props) {
  device_properties_.swap(props);
  CreateNetworkHealthState();
}

void NetworkHealthService::CreateNetworkHealthState() {
  // If the device information has not been collected, the NetworkHealthState
  // cannot be created.
  if (device_properties_.empty())
    return;

  std::vector<mojom::NetworkPtr> prev_networks =
      mojo::Clone(network_health_state_.networks);

  network_health_state_.networks.clear();

  std::map<network_config::mojom::NetworkType,
           network_config::mojom::DeviceStatePropertiesPtr>
      device_type_map;

  // This function only supports one Network structure per underlying device. If
  // this assumption changes, this function will need to be reworked.
  for (const auto& d : device_properties_) {
    device_type_map[d->type] = mojo::Clone(d);
  }

  // For each NetworkStateProperties, create a Network structure using the
  // underlying DeviceStateProperties. Remove devices from the type map that
  // have an associated NetworkStateProperties.
  for (const auto& net_prop : network_properties_) {
    auto device_iter = device_type_map.find(net_prop->type);
    if (device_iter == device_type_map.end()) {
      continue;
    }
    network_health_state_.networks.push_back(
        CreateNetwork(device_iter->second, net_prop));
    device_type_map.erase(device_iter);
  }

  // For the remaining devices that do not have associated
  // NetworkStateProperties, create Network structures.
  for (const auto& device_prop : device_type_map) {
    // Devices that have an kUnavailable state are not valid.
    if (device_prop.second->device_state ==
        network_config::mojom::DeviceStateType::kUnavailable) {
      NET_LOG(ERROR) << "Device in unexpected unavailable state: "
                     << device_prop.second->type;
      continue;
    }

    network_health_state_.networks.push_back(
        CreateNetwork(device_prop.second, nullptr));
  }

  UpdateTrackedGuids();

  if (!NetworkListsMatch(prev_networks, network_health_state_.networks))
    NotifyObserversNetworkListChanged();
}

void NetworkHealthService::RefreshNetworkHealthState() {
  RequestNetworkStateList();
  RequestDeviceStateList();
}

void NetworkHealthService::RequestNetworkStateList() {
  remote_cros_network_config_->GetNetworkStateList(
      network_config::mojom::NetworkFilter::New(
          network_config::mojom::FilterType::kActive,
          network_config::mojom::NetworkType::kAll,
          network_config::mojom::kNoLimit),
      base::BindOnce(&NetworkHealthService::OnNetworkStateListReceived,
                     base::Unretained(this)));
}

void NetworkHealthService::RequestDeviceStateList() {
  remote_cros_network_config_->GetDeviceStateList(
      base::BindOnce(&NetworkHealthService::OnDeviceStateListReceived,
                     base::Unretained(this)));
}

const mojom::NetworkPtr* NetworkHealthService::FindMatchingNetwork(
    const std::string& guid) const {
  for (const mojom::NetworkPtr& network : network_health_state_.networks) {
    if (!network->guid) {
      continue;
    }
    if (network->guid.value() == guid) {
      return &network;
    }
  }
  return nullptr;
}

void NetworkHealthService::HandleNetworkEventsForActiveNetworks(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr>
        active_networks) {
  for (const auto& network_state : active_networks) {
    const mojom::NetworkPtr* const network =
        FindMatchingNetwork(network_state->guid);
    // Fire an event if the network is new or a connection state change
    // occurred.
    if (!network || ConnectionStateChanged(*network, network_state)) {
      NotifyObserversConnectionStateChanged(
          network_state->guid,
          ConnectionStateToNetworkState(network_state->connection_state));
    }
    // Fire an event if the network is new or a signal strength change occurred.
    if (!network || SignalStrengthChanged(*network, network_state)) {
      NotifyObserversSignalStrengthChanged(
          network_state->guid,
          network_config::GetWirelessSignalStrength(network_state.get()));
    }
  }
}

void NetworkHealthService::HandleNetworkEventsForInactiveNetworks(
    network_config::mojom::NetworkStatePropertiesPtr network_state) {
  // Ensure that the connection state is no longer active.
  if (ConnectionStateToNetworkState(network_state->connection_state) !=
      mojom::NetworkState::kNotConnected) {
    return;
  }
  const mojom::NetworkPtr* const network =
      FindMatchingNetwork(network_state->guid);
  if (!network) {
    return;
  }
  // Fire an event if the network was previously active.
  if (ConnectionStateChanged(*network, network_state)) {
    NotifyObserversConnectionStateChanged(
        network_state->guid,
        ConnectionStateToNetworkState(network_state->connection_state));
  }
}

void NetworkHealthService::NotifyObserversConnectionStateChanged(
    const std::string& guid,
    mojom::NetworkState state) {
  for (auto& observer : observers_) {
    observer->OnConnectionStateChanged(guid, state);
  }
}

void NetworkHealthService::NotifyObserversSignalStrengthChanged(
    const std::string& guid,
    int signal_strength) {
  for (auto& observer : observers_) {
    observer->OnSignalStrengthChanged(guid,
                                      mojom::UInt32Value::New(signal_strength));
  }
}

void NetworkHealthService::NotifyObserversNetworkListChanged() {
  for (auto& observer : observers_) {
    observer->OnNetworkListChanged(mojo::Clone(network_health_state_.networks));
  }
}

bool NetworkHealthService::ConnectionStateChanged(
    const mojom::NetworkPtr& network,
    const network_config::mojom::NetworkStatePropertiesPtr& network_state) {
  auto state = ConnectionStateToNetworkState(network_state->connection_state);
  if (state == network->state) {
    return false;
  }
  return true;
}

bool NetworkHealthService::SignalStrengthChanged(
    const mojom::NetworkPtr& network,
    const network_config::mojom::NetworkStatePropertiesPtr& network_state) {
  if (!network_config::NetworkStateMatchesType(
          network_state.get(), network_config::mojom::NetworkType::kWireless)) {
    return false;
  }
  DCHECK(network->signal_strength);

  auto current = network_config::GetWirelessSignalStrength(network_state.get());
  uint32_t previous = network->signal_strength->value;
  if (std::abs(1.0 * (current - (int)previous)) <=
      kMaxSignalStrengthFluctuationTolerance) {
    return false;
  }
  return true;
}

void NetworkHealthService::AnalyzeSignalStrength() {
  std::set<std::string> analyzed_networks;
  for (auto& network : network_health_state_.networks) {
    if (!network->guid.has_value() || network->signal_strength.is_null())
      continue;

    auto guid = network->guid.value();
    auto& tracker = signal_strength_trackers_[guid];
    tracker.AddSample(static_cast<uint8_t>(network->signal_strength->value));

    auto stats = mojom::SignalStrengthStats::New();
    stats->average = tracker.Average();
    stats->deviation = tracker.StdDev();
    stats->samples = tracker.Samples();
    network->signal_strength_stats = std::move(stats);
    analyzed_networks.insert(guid);
  }

  // Remove all entries that are not actively being analyzed.
  for (auto it = signal_strength_trackers_.begin();
       it != signal_strength_trackers_.end();) {
    if (!analyzed_networks.count(it->first)) {
      it = signal_strength_trackers_.erase(it);
    } else {
      ++it;
    }
  }
}

bool NetworkHealthService::IsActive(const mojom::NetworkPtr& network) {
  return network->state == mojom::NetworkState::kConnecting ||
         network->state == mojom::NetworkState::kPortal ||
         network->state == mojom::NetworkState::kConnected ||
         network->state == mojom::NetworkState::kOnline;
}

void NetworkHealthService::UpdateTrackedGuids() {
  for (auto& network : network_health_state_.networks) {
    if (network->guid.has_value() && IsActive(network)) {
      guid_to_active_time_[network->guid.value()] = base::Time::Now();
    }
  }
  for (auto it = guid_to_active_time_.begin();
       it != guid_to_active_time_.end();) {
    if (base::Time::Now() - it->second > kUpdateTrackedGuidsInterval) {
      it = guid_to_active_time_.erase(it);
    } else {
      it++;
    }
  }
}

}  // namespace ash::network_health
