// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_NETWORK_NETWORK_STATUS_LISTENER_H_
#define COMPONENTS_DOWNLOAD_NETWORK_NETWORK_STATUS_LISTENER_H_

#include "base/macros.h"
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
    virtual void OnNetworkChanged(network::mojom::ConnectionType type) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Starts to listen to network changes.
  virtual void Start(Observer* observer) = 0;

  // Stops to listen to network changes.
  virtual void Stop() = 0;

  // Gets the current connection type.
  virtual network::mojom::ConnectionType GetConnectionType() = 0;

  virtual ~NetworkStatusListener();

 protected:
  NetworkStatusListener();

  // The only observer that listens to connection type change.
  Observer* observer_ = nullptr;

  // The current network status.
  network::mojom::ConnectionType connection_type_ =
      network::mojom::ConnectionType::CONNECTION_UNKNOWN;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkStatusListener);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_NETWORK_NETWORK_STATUS_LISTENER_H_
