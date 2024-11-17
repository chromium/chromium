// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_NETWORK_NETWORK_STATUS_LISTENER_H_
#define COMPONENTS_DOWNLOAD_NETWORK_NETWORK_STATUS_LISTENER_H_

#include "base/memory/raw_ptr.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"

namespace download {

// Monitor and propagate network status change events.
// Base class only manages the observer pointer, derived class should override
// to provide actual network hook to monitor the changes, and call base class
// virtual functions.
class NetworkStatusListener {
 public:
  // Observer to receive network connection type change notifications.
  class Observer {
   public:
    // Called after the NetworkStatusListener is initialized and ready to use.
    virtual void OnNetworkStatusReady(network::mojom::ConnectionType type) = 0;

    // Called when the network type is changed.
    virtual void OnNetworkChanged(network::mojom::ConnectionType type) = 0;

    Observer() = default;

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

   protected:
    virtual ~Observer() = default;
  };

  NetworkStatusListener(const NetworkStatusListener&) = delete;
  NetworkStatusListener& operator=(const NetworkStatusListener&) = delete;

  virtual ~NetworkStatusListener();

  // Starts to listen to network changes.
  virtual void Start(Observer* observer) = 0;

  // Stops to listen to network changes.
  virtual void Stop() = 0;

  // Gets the current connection type.
  virtual network::mojom::ConnectionType GetConnectionType() = 0;

 protected:
  NetworkStatusListener();

  // The only observer that listens to connection type change. Must outlive this
  // class.
  raw_ptr<Observer> observer_ = nullptr;

  // The current network status.
  network::mojom::ConnectionType connection_type_ =
      network::mojom::ConnectionType::CONNECTION_UNKNOWN;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_NETWORK_NETWORK_STATUS_LISTENER_H_
