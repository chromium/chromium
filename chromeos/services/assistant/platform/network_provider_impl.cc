// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/network_provider_impl.h"

using assistant_client::NetworkProvider;
using ConnectionStatus = assistant_client::NetworkProvider::ConnectionStatus;

namespace chromeos {
namespace assistant {

NetworkProviderImpl::NetworkProviderImpl(
    network::NetworkConnectionTracker* network_connection_tracker)
    : network_connection_tracker_(network_connection_tracker),
      weak_factory_(this) {
  if (network_connection_tracker_) {
    network_connection_tracker_->AddNetworkConnectionObserver(this);
    network_connection_tracker_->GetConnectionType(
        &connection_type_,
        base::BindOnce(&NetworkProviderImpl::OnConnectionChanged,
                       weak_factory_.GetWeakPtr()));
  }
}

NetworkProviderImpl::~NetworkProviderImpl() {
  if (network_connection_tracker_)
    network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

void NetworkProviderImpl::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  connection_type_ = type;
}

ConnectionStatus NetworkProviderImpl::GetConnectionStatus() {
  // TODO(updowndota): Check actual internect connectivity in addition to the
  // physical connectivity.
  switch (connection_type_) {
    case network::mojom::ConnectionType::CONNECTION_UNKNOWN:
      return ConnectionStatus::UNKNOWN;
    case network::mojom::ConnectionType::CONNECTION_ETHERNET:
    case network::mojom::ConnectionType::CONNECTION_WIFI:
    case network::mojom::ConnectionType::CONNECTION_2G:
    case network::mojom::ConnectionType::CONNECTION_3G:
    case network::mojom::ConnectionType::CONNECTION_4G:
    case network::mojom::ConnectionType::CONNECTION_BLUETOOTH:
      return ConnectionStatus::CONNECTED;
    case network::mojom::ConnectionType::CONNECTION_NONE:
      return ConnectionStatus::DISCONNECTED_FROM_INTERNET;
  }
}

// Mdns responder is not supported in ChromeOS.
assistant_client::MdnsResponder* NetworkProviderImpl::GetMdnsResponder() {
  return nullptr;
}

}  // namespace assistant
}  // namespace chromeos
