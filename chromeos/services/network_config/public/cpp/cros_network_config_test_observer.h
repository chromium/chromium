// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_OBSERVER_H_
#define CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_OBSERVER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace network_config {

class CrosNetworkConfigTestObserver : public mojom::CrosNetworkConfigObserver {
 public:
  CrosNetworkConfigTestObserver();
  ~CrosNetworkConfigTestObserver() override;

  mojo::PendingRemote<mojom::CrosNetworkConfigObserver> GenerateRemote();

  // mojom::CrosNetworkConfigObserver:
  void OnActiveNetworksChanged(
      std::vector<mojom::NetworkStatePropertiesPtr> networks) override;
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network)
      override;
  void OnNetworkStateListChanged() override;
  void OnDeviceStateListChanged() override;
  void OnVpnProvidersChanged() override;
  void OnNetworkCertificatesChanged() override;

  int active_networks_changed() const { return active_networks_changed_; }
  int network_state_list_changed() const { return network_state_list_changed_; }
  int device_state_list_changed() const { return device_state_list_changed_; }
  int vpn_providers_changed() const { return vpn_providers_changed_; }
  int network_certificates_changed() const {
    return network_certificates_changed_;
  }

  int GetNetworkChangedCount(const std::string& guid) const;
  void ResetNetworkChanges();

  mojo::Receiver<mojom::CrosNetworkConfigObserver>& receiver() {
    return receiver_;
  }

  void FlushForTesting();

 private:
  mojo::Receiver<mojom::CrosNetworkConfigObserver> receiver_{this};
  int active_networks_changed_ = 0;
  std::map<std::string, int> networks_changed_;
  int network_state_list_changed_ = 0;
  int device_state_list_changed_ = 0;
  int vpn_providers_changed_ = 0;
  int network_certificates_changed_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CrosNetworkConfigTestObserver);
};

}  // namespace network_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_OBSERVER_H_
