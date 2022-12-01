// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/hidden_network_metrics_helper.h"

#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash {

HiddenNetworkMetricsHelper::HiddenNetworkMetricsHelper() = default;

HiddenNetworkMetricsHelper::~HiddenNetworkMetricsHelper() = default;

void HiddenNetworkMetricsHelper::Init(
    NetworkConfigurationHandler* network_configuration_handler) {
  if (network_configuration_handler)
    network_configuration_observation_.Observe(network_configuration_handler);
}

void HiddenNetworkMetricsHelper::OnConfigurationCreated(
    const std::string& service_path,
    const std::string& guid) {
  const NetworkState* network_state =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          guid);

  if (!network_state || network_state->GetNetworkTechnologyType() !=
                            NetworkState::NetworkTechnologyType::kWiFi) {
    return;
  }

  if (LoginState::IsInitialized() && LoginState::Get()->IsUserLoggedIn()) {
    base::UmaHistogramBoolean("Network.Ash.WiFi.Hidden.LoggedIn",
                              network_state->hidden_ssid());
  } else {
    base::UmaHistogramBoolean("Network.Ash.WiFi.Hidden.NotLoggedIn",
                              network_state->hidden_ssid());
  }
}

}  // namespace ash
