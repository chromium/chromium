// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_NETWORK_PROVIDER_IMPL_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_NETWORK_PROVIDER_IMPL_H_

#include <vector>

#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/services/libassistant/public/mojom/platform_delegate.mojom-forward.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace libassistant {

class COMPONENT_EXPORT(ASSISTANT_SERVICE) NetworkProviderImpl
    : public assistant_client::NetworkProvider,
      public network_config::mojom::CrosNetworkConfigObserver {
 public:
  NetworkProviderImpl();

  NetworkProviderImpl(const NetworkProviderImpl&) = delete;
  NetworkProviderImpl& operator=(const NetworkProviderImpl&) = delete;

  ~NetworkProviderImpl() override;

  void Initialize(mojom::PlatformDelegate* platform_delegate);

  // assistant_client::NetworkProvider:
  ConnectionStatus GetConnectionStatus() override;
  assistant_client::MdnsResponder* GetMdnsResponder() override;

  // network_config::mojom::CrosNetworkConfigObserver:
  void OnActiveNetworksChanged(
      std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks)
      override;
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network)
      override {}
  void OnNetworkStateListChanged() override {}
  void OnDeviceStateListChanged() override {}
  void OnVpnProvidersChanged() override {}
  void OnNetworkCertificatesChanged() override {}
  void OnPoliciesApplied(const std::string& userhash) override {}

 private:
  ConnectionStatus connection_status_;
  mojo::Receiver<network_config::mojom::CrosNetworkConfigObserver> receiver_{
      this};
  mojo::Remote<network_config::mojom::CrosNetworkConfig>
      cros_network_config_remote_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_NETWORK_PROVIDER_IMPL_H_
