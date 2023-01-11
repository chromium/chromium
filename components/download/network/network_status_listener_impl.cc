// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/network/network_status_listener_impl.h"

#include "base/functional/bind.h"

namespace download {

NetworkStatusListenerImpl::NetworkStatusListenerImpl(
    network::NetworkConnectionTracker* network_connection_tracker)
    : network_connection_tracker_(network_connection_tracker) {}

NetworkStatusListenerImpl::~NetworkStatusListenerImpl() = default;

void NetworkStatusListenerImpl::Start(
    NetworkStatusListener::Observer* observer) {
  NetworkStatusListener::Start(observer);
  network_connection_tracker_->AddNetworkConnectionObserver(this);
  bool sync = network_connection_tracker_->GetConnectionType(
      &connection_type_,
      base::BindOnce(&NetworkStatusListenerImpl::OnNetworkStatusReady,
                     weak_ptr_factory_.GetWeakPtr()));
  if (sync)
    observer_->OnNetworkStatusReady(connection_type_);
}

void NetworkStatusListenerImpl::Stop() {
  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
  NetworkStatusListener::Stop();
}

network::mojom::ConnectionType NetworkStatusListenerImpl::GetConnectionType() {
  return connection_type_;
}

void NetworkStatusListenerImpl::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK(observer_);
  connection_type_ = type;
  observer_->OnNetworkChanged(type);
}

void NetworkStatusListenerImpl::OnNetworkStatusReady(
    network::mojom::ConnectionType type) {
  DCHECK(observer_);
  connection_type_ = type;
  observer_->OnNetworkStatusReady(type);
}

}  // namespace download
