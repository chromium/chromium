// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_HIDDEN_NETWORK_METRICS_HELPER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_HIDDEN_NETWORK_METRICS_HELPER_H_

#include <string>

#include "base/component_export.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_configuration_observer.h"

namespace ash {

// This class is used to track the configuration of WiFi networks and emit UMA
// metrics that capture when these networks were configured and whether they
// were configured to be hidden.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) HiddenNetworkMetricsHelper
    : public NetworkConfigurationObserver {
 public:
  HiddenNetworkMetricsHelper();
  HiddenNetworkMetricsHelper(const HiddenNetworkMetricsHelper&) = delete;
  HiddenNetworkMetricsHelper& operator=(const HiddenNetworkMetricsHelper&) =
      delete;
  ~HiddenNetworkMetricsHelper() override;

  void Init(NetworkConfigurationHandler* network_configuration_handler);

 private:
  friend class HiddenNetworkMetricsHelperTest;

  // NetworkConfigurationObserver:
  void OnConfigurationCreated(const std::string& service_path,
                              const std::string& guid) override;

  base::ScopedObservation<NetworkConfigurationHandler,
                          NetworkConfigurationObserver>
      network_configuration_observation_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_HIDDEN_NETWORK_METRICS_HELPER_H_
