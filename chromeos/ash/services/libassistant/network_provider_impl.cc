// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/network_provider_impl.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "chromeos/ash/services/libassistant/public/mojom/platform_delegate.mojom.h"
#include "chromeos/services/network_config/public/mojom/constants.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"

using ConnectionStatus = assistant_client::NetworkProvider::ConnectionStatus;

namespace ash::libassistant {

namespace network_config = ::chromeos::network_config;

NetworkProviderImpl::NetworkProviderImpl()
    : connection_status_(ConnectionStatus::UNKNOWN) {}

void NetworkProviderImpl::Initialize(
    mojom::PlatformDelegate* platform_delegate) {
  platform_delegate->BindNetworkConfig(
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
  const bool is_any_network_online = base::Contains(
      networks, network_config::mojom::ConnectionStateType::kOnline,
      &network_config::mojom::NetworkStateProperties::connection_state);

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

}  // namespace ash::libassistant
