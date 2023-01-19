// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_observer.h"

#include "chromeos/ash/services/network_config/cros_network_config.h"
#include "chromeos/services/network_config/public/mojom/constants.mojom.h"

namespace ash::network_config {

namespace mojom = ::chromeos::network_config::mojom;

CrosNetworkConfigTestObserver::CrosNetworkConfigTestObserver() = default;
CrosNetworkConfigTestObserver::~CrosNetworkConfigTestObserver() = default;

mojo::PendingRemote<mojom::CrosNetworkConfigObserver>
CrosNetworkConfigTestObserver::GenerateRemote() {
  return receiver().BindNewPipeAndPassRemote();
}

int CrosNetworkConfigTestObserver::GetNetworkChangedCount(
    const std::string& guid) const {
  const auto iter = guid_to_networks_changed_count_map_.find(guid);
  if (iter == guid_to_networks_changed_count_map_.end())
    return 0;
  return iter->second;
}

int CrosNetworkConfigTestObserver::GetPolicyAppliedCount(
    const std::string& userhash) const {
  const auto iter = userhash_to_policies_applied_count_map_.find(userhash);
  if (iter == userhash_to_policies_applied_count_map_.end())
    return 0;
  return iter->second;
}

void CrosNetworkConfigTestObserver::OnActiveNetworksChanged(
    std::vector<mojom::NetworkStatePropertiesPtr> networks) {
  active_networks_changed_++;
}

void CrosNetworkConfigTestObserver::OnNetworkStateChanged(
    mojom::NetworkStatePropertiesPtr network) {
  guid_to_networks_changed_count_map_[network->guid]++;
}

void CrosNetworkConfigTestObserver::OnNetworkStateListChanged() {
  network_state_list_changed_++;
}

void CrosNetworkConfigTestObserver::OnDeviceStateListChanged() {
  device_state_list_changed_++;
}

void CrosNetworkConfigTestObserver::OnVpnProvidersChanged() {
  vpn_providers_changed_++;
}

void CrosNetworkConfigTestObserver::OnNetworkCertificatesChanged() {
  network_certificates_changed_++;
}

void CrosNetworkConfigTestObserver::OnPoliciesApplied(
    const std::string& userhash) {
  userhash_to_policies_applied_count_map_[userhash]++;
}

void CrosNetworkConfigTestObserver::ResetNetworkChanges() {
  active_networks_changed_ = 0;
  guid_to_networks_changed_count_map_.clear();
  userhash_to_policies_applied_count_map_.clear();
  network_state_list_changed_ = 0;
  device_state_list_changed_ = 0;
  vpn_providers_changed_ = 0;
  network_certificates_changed_ = 0;
}

void CrosNetworkConfigTestObserver::FlushForTesting() {
  receiver_.FlushForTesting();
}

}  // namespace ash::network_config
