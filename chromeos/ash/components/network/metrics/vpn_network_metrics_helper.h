// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_VPN_NETWORK_METRICS_HELPER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_VPN_NETWORK_METRICS_HELPER_H_

#include <string>

#include "base/component_export.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_configuration_observer.h"

namespace ash {

// This class is responsible for tracking the creation of VPN networks,
// recording the provider type and whether it was configured manually or via
// policy and reporting this information to the UMA backend.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) VpnNetworkMetricsHelper
    : public NetworkConfigurationObserver {
 public:
  VpnNetworkMetricsHelper();
  VpnNetworkMetricsHelper(const VpnNetworkMetricsHelper&) = delete;
  VpnNetworkMetricsHelper& operator=(const VpnNetworkMetricsHelper&) = delete;
  ~VpnNetworkMetricsHelper() override;

  void Init(NetworkConfigurationHandler* network_configuration_handler);

 private:
  friend class VpnNetworkMetricsHelperTest;

  // The different configuration sources for VPN networks. These values are used
  // when reporting to the UMA backend and should not be renumbered or renamed.
  // The name "VPNConfigurationSource" is cased to match other metrics.
  enum class VPNConfigurationSource {
    kConfiguredManually = 0,
    kConfiguredByPolicy = 1,
    kMaxValue = kConfiguredByPolicy,
  };

  // NetworkConfigurationObserver:
  void OnConfigurationCreated(const std::string& service_path,
                              const std::string& guid) override;

  base::ScopedObservation<NetworkConfigurationHandler,
                          NetworkConfigurationObserver>
      network_configuration_observation_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_VPN_NETWORK_METRICS_HELPER_H_
