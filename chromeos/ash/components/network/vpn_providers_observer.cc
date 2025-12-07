// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/vpn_providers_observer.h"

#include "base/functional/bind.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace ash {

VpnProvidersObserver::VpnProvidersObserver(
    VpnProvidersObserver::Delegate* delegate)
    : delegate_(delegate) {
  network_config::BindToInProcessInstance(
      cros_network_config_.BindNewPipeAndPassReceiver());
  cros_network_config_->AddObserver(
      cros_network_config_observer_.BindNewPipeAndPassRemote());
}

VpnProvidersObserver::~VpnProvidersObserver() = default;

void VpnProvidersObserver::OnVpnProvidersChanged() {
  cros_network_config_->GetVpnProviders(base::BindOnce(
      &VpnProvidersObserver::OnGetVpnProviders, weak_factory_.GetWeakPtr()));
}

void VpnProvidersObserver::OnGetVpnProviders(
    std::vector<chromeos::network_config::mojom::VpnProviderPtr>
        vpn_providers) {
  if (!delegate_) {
    return;
  }
  base::flat_set<std::string> vpn_extensions;
  for (const auto& vpn_provider : vpn_providers) {
    if (vpn_provider->type ==
        chromeos::network_config::mojom::VpnType::kExtension) {
      vpn_extensions.insert(vpn_provider->app_id);
    }
  }
  delegate_->OnVpnExtensionsChanged(std::move(vpn_extensions));
}

}  // namespace ash
