// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/public/cpp/cros_network_config_test_observer.h"

#include "chromeos/services/network_config/cros_network_config.h"
#include "chromeos/services/network_config/public/mojom/constants.mojom.h"

namespace chromeos {
namespace network_config {

CrosNetworkConfigTestObserver::CrosNetworkConfigTestObserver() = default;
CrosNetworkConfigTestObserver::~CrosNetworkConfigTestObserver() = default;

mojo::PendingRemote<mojom::CrosNetworkConfigObserver>
CrosNetworkConfigTestObserver::GenerateRemote() {
  return receiver().BindNewPipeAndPassRemote();
}

int CrosNetworkConfigTestObserver::GetNetworkChangedCount(
    const std::string& guid) const {
  const auto iter = networks_changed_.find(guid);
  if (iter == networks_changed_.end())
    return 0;
  return iter->second;
}

void CrosNetworkConfigTestObserver::OnActiveNetworksChanged(
    std::vector<mojom::NetworkStatePropertiesPtr> networks) {
  active_networks_changed_++;
}

void CrosNetworkConfigTestObserver::OnNetworkStateChanged(
    chromeos::network_config::mojom::NetworkStatePropertiesPtr network) {
  networks_changed_[network->guid]++;
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

void CrosNetworkConfigTestObserver::ResetNetworkChanges() {
  active_networks_changed_ = 0;
  networks_changed_.clear();
  network_state_list_changed_ = 0;
  device_state_list_changed_ = 0;
  vpn_providers_changed_ = 0;
  network_certificates_changed_ = 0;
}

void CrosNetworkConfigTestObserver::FlushForTesting() {
  receiver_.FlushForTesting();
}

}  // namespace network_config
}  // namespace chromeos
