// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/public/cpp/fake_cros_network_config.h"

#include <memory>

#include "base/run_loop.h"

namespace chromeos::network_config {

FakeCrosNetworkConfig::FakeCrosNetworkConfig() = default;
FakeCrosNetworkConfig::~FakeCrosNetworkConfig() = default;

void FakeCrosNetworkConfig::AddObserver(
    mojo::PendingRemote<mojom::CrosNetworkConfigObserver> observer) {
  observers_.Add(std::move(observer));
}

void FakeCrosNetworkConfig::GetNetworkStateList(
    mojom::NetworkFilterPtr filter,
    GetNetworkStateListCallback callback) {
  std::move(callback).Run(
      GetFilteredNetworkList(filter->network_type, filter->filter));
}

void FakeCrosNetworkConfig::GetDeviceStateList(
    GetDeviceStateListCallback callback) {
  std::move(callback).Run(mojo::Clone(device_properties_));
}

void FakeCrosNetworkConfig::GetManagedProperties(
    const std::string& guid,
    GetManagedPropertiesCallback callback) {
  auto it = guid_to_managed_properties_.find(guid);
  if (it != guid_to_managed_properties_.end()) {
    std::move(callback).Run(it->second.Clone());
    return;
  }
  std::move(callback).Run(nullptr);
}

void FakeCrosNetworkConfig::RequestNetworkScan(mojom::NetworkType type) {
  scan_count_[type]++;
}

void FakeCrosNetworkConfig::GetGlobalPolicy(GetGlobalPolicyCallback callback) {
  if (!global_policy_) {
    global_policy_ = mojom::GlobalPolicy::New();
  }

  std::move(callback).Run(global_policy_.Clone());
  base::RunLoop().RunUntilIdle();
}

void FakeCrosNetworkConfig::GetVpnProviders(GetVpnProvidersCallback callback) {
  std::vector<mojom::VpnProviderPtr> providers;
  std::move(callback).Run(std::move(providers));
}

void FakeCrosNetworkConfig::CreateCustomApn(const std::string& network_guid,
                                            mojom::ApnPropertiesPtr apn,
                                            CreateCustomApnCallback callback) {
  pending_create_custom_apn_callbacks_.push(
      std::make_pair(std::move(callback), std::move(apn)));
}

void FakeCrosNetworkConfig::CreateExclusivelyEnabledCustomApn(
    const std::string& network_guid,
    mojom::ApnPropertiesPtr apn,
    CreateExclusivelyEnabledCustomApnCallback callback) {
  pending_create_exclusively_enabled_custom_apn_callbacks_.push(
      std::make_pair(std::move(callback), std::move(apn)));
}

void FakeCrosNetworkConfig::InvokePendingCreateCustomApnCallback(bool success) {
  if (success) {
    custom_apns_.push_back(
        std::move(pending_create_custom_apn_callbacks_.front().second));
  }
  std::move(pending_create_custom_apn_callbacks_.front().first).Run(success);
  pending_create_custom_apn_callbacks_.pop();
}

void FakeCrosNetworkConfig::SetDeviceProperties(
    mojom::DeviceStatePropertiesPtr device_properties) {
  AddOrReplaceDevice(std::move(device_properties));
  for (auto& observer : observers_) {
    observer->OnDeviceStateListChanged();
  }
  base::RunLoop().RunUntilIdle();
}

void FakeCrosNetworkConfig::SetGlobalPolicy(
    bool allow_only_policy_cellular_networks,
    bool dns_queries_monitored,
    bool report_xdr_events_enabled) {
  global_policy_ = mojom::GlobalPolicy::New();
  global_policy_->allow_only_policy_cellular_networks =
      allow_only_policy_cellular_networks;
  global_policy_->dns_queries_monitored = dns_queries_monitored;
  global_policy_->report_xdr_events_enabled = report_xdr_events_enabled;
  for (auto& observer : observers_) {
    observer->OnPoliciesApplied(/*userhash=*/std::string());
  }
  base::RunLoop().RunUntilIdle();
}

void FakeCrosNetworkConfig::SetNetworkState(
    const std::string& guid,
    mojom::ConnectionStateType connection_state_type) {
  for (auto& network : visible_networks_) {
    if (network->guid == guid) {
      network->connection_state = connection_state_type;
      break;
    }
  }
  for (auto& observer : observers_) {
    observer->OnActiveNetworksChanged(GetFilteredNetworkList(
        mojom::NetworkType::kAll, mojom::FilterType::kActive));
  }
  base::RunLoop().RunUntilIdle();
}

void FakeCrosNetworkConfig::AddNetworkAndDevice(
    mojom::NetworkStatePropertiesPtr network) {
  auto device_properties = mojom::DeviceStateProperties::New();
  device_properties->type = network->type;
  device_properties->device_state = mojom::DeviceStateType::kEnabled;

  visible_networks_.push_back(std::move(network));
  AddOrReplaceDevice(std::move(device_properties));

  for (auto& observer : observers_) {
    observer->OnDeviceStateListChanged();
    observer->OnActiveNetworksChanged(GetFilteredNetworkList(
        mojom::NetworkType::kAll, mojom::FilterType::kActive));
  }
  base::RunLoop().RunUntilIdle();
}

void FakeCrosNetworkConfig::UpdateNetworkProperties(
    mojom::NetworkStatePropertiesPtr network) {
  bool is_found = false;
  for (unsigned int i = 0; i < visible_networks_.size(); i++) {
    if (visible_networks_[i]->guid == network->guid) {
      visible_networks_[i] = mojo::Clone(network);
      is_found = true;
      break;
    }
  }

  if (!is_found) {
    return;
  }

  for (auto& observer : observers_) {
    observer->OnActiveNetworksChanged(GetFilteredNetworkList(
        mojom::NetworkType::kAll, mojom::FilterType::kActive));
  }
  base::RunLoop().RunUntilIdle();
}

void FakeCrosNetworkConfig::AddManagedProperties(
    const std::string& guid,
    mojom::ManagedPropertiesPtr managed_properties) {
  guid_to_managed_properties_[guid] = std::move(managed_properties);
}

void FakeCrosNetworkConfig::ClearNetworksAndDevices() {
  visible_networks_.clear();
  device_properties_.clear();
  for (auto& observer : observers_) {
    observer->OnDeviceStateListChanged();
    observer->OnActiveNetworksChanged({});
  }
  base::RunLoop().RunUntilIdle();
}

void FakeCrosNetworkConfig::RemoveNthNetworks(size_t index) {
  DCHECK(index < visible_networks_.size() && index >= 0);
  visible_networks_.erase(visible_networks_.begin() + index);
  for (auto& observer : observers_) {
    observer->OnDeviceStateListChanged();
  }
  base::RunLoop().RunUntilIdle();
}

int FakeCrosNetworkConfig::GetScanCount(mojom::NetworkType type) {
  return scan_count_[type];
}

mojo::PendingRemote<mojom::CrosNetworkConfig>
FakeCrosNetworkConfig::GetPendingRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void FakeCrosNetworkConfig::AddOrReplaceDevice(
    mojom::DeviceStatePropertiesPtr device_properties) {
  auto it = std::find_if(
      device_properties_.begin(), device_properties_.end(),
      [&device_properties](const mojom::DeviceStatePropertiesPtr& p) {
        return p->type == device_properties->type;
      });
  if (it != device_properties_.end()) {
    (*it).Swap(&device_properties);
  } else {
    device_properties_.insert(device_properties_.begin(),
                              std::move(device_properties));
  }
}

std::vector<mojom::NetworkStatePropertiesPtr>
FakeCrosNetworkConfig::GetFilteredNetworkList(mojom::NetworkType network_type,
                                              mojom::FilterType filter_type) {
  std::vector<mojom::NetworkStatePropertiesPtr> result;
  for (const auto& network : visible_networks_) {
    if (network_type != mojom::NetworkType::kAll &&
        network_type != network->type) {
      continue;
    }
    if (filter_type == mojom::FilterType::kActive &&
        network->connection_state ==
            mojom::ConnectionStateType::kNotConnected) {
      continue;
    }
    result.push_back(network.Clone());
  }
  return result;
}

}  // namespace chromeos::network_config
