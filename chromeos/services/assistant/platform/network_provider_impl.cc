// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/network_provider_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "chromeos/services/network_config/public/mojom/constants.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"

using ConnectionStatus = assistant_client::NetworkProvider::ConnectionStatus;
using NetworkStatePropertiesPtr =
    chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using ConnectionStateType =
    chromeos::network_config::mojom::ConnectionStateType;

namespace chromeos {
namespace assistant {

NetworkProviderImpl::NetworkProviderImpl(mojom::Client* client)
    : connection_status_(ConnectionStatus::UNKNOWN) {
  if (!client)
    return;
  client->RequestNetworkConfig(
      cros_network_config_remote_.BindNewPipeAndPassReceiver());
  cros_network_config_remote_->AddObserver(
      receiver_.BindNewPipeAndPassRemote());
  cros_network_config_remote_->GetNetworkStateList(
      network_config::mojom::NetworkFilter::New(
          network_config::mojom::FilterType::kActive,
          network_config::mojom::NetworkType::kAll,
          network_config::mojom::kNoLimit),
      base::BindOnce(&NetworkProviderImpl::OnActiveNetworksChanged,
                     base::Unretained(this)));
}

NetworkProviderImpl::~NetworkProviderImpl() = default;

void NetworkProviderImpl::OnActiveNetworksChanged(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks) {
  const bool is_any_network_online =
      std::any_of(networks.begin(), networks.end(), [](const auto& network) {
        return network->connection_state == ConnectionStateType::kOnline;
      });

  if (is_any_network_online)
    connection_status_ = ConnectionStatus::CONNECTED;
  else
    connection_status_ = ConnectionStatus::DISCONNECTED_FROM_INTERNET;
}

ConnectionStatus NetworkProviderImpl::GetConnectionStatus() {
  return connection_status_;
}

// Mdns responder is not supported in ChromeOS.
assistant_client::MdnsResponder* NetworkProviderImpl::GetMdnsResponder() {
  return nullptr;
}

}  // namespace assistant
}  // namespace chromeos
