// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_NETWORK_NETWORK_STATUS_LISTENER_IMPL_H_
#define COMPONENTS_DOWNLOAD_NETWORK_NETWORK_STATUS_LISTENER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/download/network/network_status_listener.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace download {

// Default implementation of NetworkStatusListener using
// NetworkConnectionTracker to listen to connectivity changes.
class NetworkStatusListenerImpl
    : public network::NetworkConnectionTracker::NetworkConnectionObserver,
      public NetworkStatusListener {
 public:
  explicit NetworkStatusListenerImpl(
      network::NetworkConnectionTracker* network_connection_tracker);

  NetworkStatusListenerImpl(const NetworkStatusListenerImpl&) = delete;
  NetworkStatusListenerImpl& operator=(const NetworkStatusListenerImpl&) =
      delete;

  ~NetworkStatusListenerImpl() override;

  // NetworkStatusListener implementation.
  void Start(NetworkStatusListener::Observer* observer) override;
  void Stop() override;
  network::mojom::ConnectionType GetConnectionType() override;

 private:
  // network::NetworkConnectionTracker::NetworkConnectionObserver.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  void OnNetworkStatusReady(network::mojom::ConnectionType type);

  raw_ptr<network::NetworkConnectionTracker> network_connection_tracker_;

  base::WeakPtrFactory<NetworkStatusListenerImpl> weak_ptr_factory_{this};
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_NETWORK_NETWORK_STATUS_LISTENER_IMPL_H_
