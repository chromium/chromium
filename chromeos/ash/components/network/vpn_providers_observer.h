// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_VPN_PROVIDERS_OBSERVER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_VPN_PROVIDERS_OBSERVER_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// Listens to |OnVpnProvidersChanged| event and informs the delegate of the
// current set of vpn extension.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) VpnProvidersObserver
    : public chromeos::network_config::CrosNetworkConfigObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnVpnExtensionsChanged(
        base::flat_set<std::string> vpn_extensions) = 0;
  };

  explicit VpnProvidersObserver(Delegate* delegate);
  ~VpnProvidersObserver() override;

  // ash::network_config::CrosNetworkConfigObserver:
  void OnVpnProvidersChanged() override;

 private:
  // Callback for CrosNetworkConfig::GetVpnProviders().
  // Extracts vpn extension ids and calls Delegate::OnVpnExtensionsChanged().
  void OnGetVpnProviders(
      std::vector<chromeos::network_config::mojom::VpnProviderPtr>
          vpn_providers);

  raw_ptr<Delegate> delegate_ = nullptr;

  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_{this};

  base::WeakPtrFactory<VpnProvidersObserver> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_VPN_PROVIDERS_OBSERVER_H_
