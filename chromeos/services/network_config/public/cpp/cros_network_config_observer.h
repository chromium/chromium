// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_OBSERVER_H_
#define CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_OBSERVER_H_

#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace chromeos::network_config {

// This class allows derived observers to override only the methods that they
// need.
class CrosNetworkConfigObserver : public mojom::CrosNetworkConfigObserver {
 public:
  ~CrosNetworkConfigObserver() override = default;

  void OnActiveNetworksChanged(
      std::vector<mojom::NetworkStatePropertiesPtr> networks) override;
  void OnNetworkStateChanged(mojom::NetworkStatePropertiesPtr network) override;
  void OnNetworkStateListChanged() override;
  void OnDeviceStateListChanged() override;
  void OnVpnProvidersChanged() override;
  void OnNetworkCertificatesChanged() override;
  void OnPoliciesApplied(const std::string& userhash) override;
};

}  // namespace chromeos::network_config

#endif  // CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_OBSERVER_H_
