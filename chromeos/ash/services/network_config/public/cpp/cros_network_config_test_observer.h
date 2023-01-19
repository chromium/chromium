// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_OBSERVER_H_
#define CHROMEOS_ASH_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_OBSERVER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::network_config {

class CrosNetworkConfigTestObserver
    : public chromeos::network_config::mojom::CrosNetworkConfigObserver {
 public:
  CrosNetworkConfigTestObserver();

  CrosNetworkConfigTestObserver(const CrosNetworkConfigTestObserver&) = delete;
  CrosNetworkConfigTestObserver& operator=(
      const CrosNetworkConfigTestObserver&) = delete;

  ~CrosNetworkConfigTestObserver() override;

  mojo::PendingRemote<
      chromeos::network_config::mojom::CrosNetworkConfigObserver>
  GenerateRemote();

  // chromeos::network_config::mojom::CrosNetworkConfigObserver:
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks) override;
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network)
      override;
  void OnNetworkStateListChanged() override;
  void OnDeviceStateListChanged() override;
  void OnVpnProvidersChanged() override;
  void OnNetworkCertificatesChanged() override;
  void OnPoliciesApplied(const std::string& userhash) override;

  int active_networks_changed() const { return active_networks_changed_; }
  int network_state_list_changed() const { return network_state_list_changed_; }
  int device_state_list_changed() const { return device_state_list_changed_; }
  int vpn_providers_changed() const { return vpn_providers_changed_; }
  int network_certificates_changed() const {
    return network_certificates_changed_;
  }

  int GetNetworkChangedCount(const std::string& guid) const;
  int GetPolicyAppliedCount(const std::string& userhash) const;
  void ResetNetworkChanges();

  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>&
  receiver() {
    return receiver_;
  }

  void FlushForTesting();

 private:
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      receiver_{this};
  int active_networks_changed_ = 0;
  base::flat_map<std::string, int> guid_to_networks_changed_count_map_;
  base::flat_map<std::string, int> userhash_to_policies_applied_count_map_;
  int network_state_list_changed_ = 0;
  int device_state_list_changed_ = 0;
  int vpn_providers_changed_ = 0;
  int network_certificates_changed_ = 0;
};

}  // namespace ash::network_config

#endif  // CHROMEOS_ASH_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_OBSERVER_H_
