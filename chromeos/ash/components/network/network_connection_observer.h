// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CONNECTION_OBSERVER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CONNECTION_OBSERVER_H_

#include <string>

#include "base/component_export.h"

namespace ash {

// Observer class for network connection events.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkConnectionObserver {
 public:
  NetworkConnectionObserver();

  NetworkConnectionObserver(const NetworkConnectionObserver&) = delete;
  NetworkConnectionObserver& operator=(const NetworkConnectionObserver&) =
      delete;

  // Called when a connection to network |service_path| is requested by
  // calling NetworkConnectionHandler::ConnectToNetwork.
  virtual void ConnectToNetworkRequested(const std::string& service_path);

  // Called when a connection request succeeds.
  virtual void ConnectSucceeded(const std::string& service_path);

  // Called when a connection request fails. Valid error names are defined in
  // NetworkConnectionHandler.
  virtual void ConnectFailed(const std::string& service_path,
                             const std::string& error_name);

  // Called when a disconnect to network |service_path| is requested by
  // calling NetworkConnectionHandler::DisconnectNetwork. Success or failure
  // for disconnect is not tracked here, observe NetworkStateHandler for state
  // changes instead.
  virtual void DisconnectRequested(const std::string& service_path);

 protected:
  virtual ~NetworkConnectionObserver();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CONNECTION_OBSERVER_H_
