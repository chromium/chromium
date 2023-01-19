// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_NETWORK_PROVIDER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_NETWORK_PROVIDER_IMPL_H_

#include <vector>

#include "chromeos/ash/services/libassistant/public/mojom/platform_delegate.mojom-forward.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::libassistant {

class COMPONENT_EXPORT(ASSISTANT_SERVICE) NetworkProviderImpl
    : public assistant_client::NetworkProvider,
      public chromeos::network_config::CrosNetworkConfigObserver {
 public:
  NetworkProviderImpl();

  NetworkProviderImpl(const NetworkProviderImpl&) = delete;
  NetworkProviderImpl& operator=(const NetworkProviderImpl&) = delete;

  ~NetworkProviderImpl() override;

  void Initialize(mojom::PlatformDelegate* platform_delegate);

  // assistant_client::NetworkProvider:
  ConnectionStatus GetConnectionStatus() override;
  assistant_client::MdnsResponder* GetMdnsResponder() override;

  // chromeos::network_config::CrosNetworkConfigObserver:
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks) override;

 private:
  ConnectionStatus connection_status_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      receiver_{this};
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      cros_network_config_remote_;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_NETWORK_PROVIDER_IMPL_H_
