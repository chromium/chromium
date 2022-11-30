// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"

namespace chromeos::network_config {

void CrosNetworkConfigObserver::OnActiveNetworksChanged(
    std::vector<mojom::NetworkStatePropertiesPtr> networks) {}

void CrosNetworkConfigObserver::OnNetworkStateChanged(
    mojom::NetworkStatePropertiesPtr network) {}

void CrosNetworkConfigObserver::OnNetworkStateListChanged() {}

void CrosNetworkConfigObserver::OnDeviceStateListChanged() {}

void CrosNetworkConfigObserver::OnVpnProvidersChanged() {}

void CrosNetworkConfigObserver::OnNetworkCertificatesChanged() {}

void CrosNetworkConfigObserver::OnPoliciesApplied(const std::string& userhash) {
}

}  // namespace chromeos::network_config
