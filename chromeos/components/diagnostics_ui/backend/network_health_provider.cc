// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/network_health_provider.h"

#include "chromeos/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"

namespace chromeos {
namespace diagnostics {

NetworkHealthProvider::NetworkHealthProvider() {
  network_config::BindToInProcessInstance(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  remote_cros_network_config_->AddObserver(
      cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());
}

NetworkHealthProvider::~NetworkHealthProvider() = default;

// TODO(michaelcheco): Use these functions to aggregate data and call Mojo API.
void NetworkHealthProvider::OnNetworkStateListChanged() {}
void NetworkHealthProvider::OnDeviceStateListChanged() {}
void NetworkHealthProvider::OnActiveNetworksChanged(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr>
        active_networks) {}
void NetworkHealthProvider::OnNetworkStateChanged(
    network_config::mojom::NetworkStatePropertiesPtr network_state) {}
void NetworkHealthProvider::OnVpnProvidersChanged() {}
void NetworkHealthProvider::OnNetworkCertificatesChanged() {}

}  // namespace diagnostics
}  // namespace chromeos
