// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_CONNECTOR_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_CONNECTOR_H_

#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/network/network_connection_handler.h"

namespace ash {

namespace tether {

// Connects to a tether network. When the user initiates a connection via the
// UI, TetherConnector receives a callback from NetworkConnectionHandler and
// initiates a connection by starting a ConnectTetheringOperation. When a
// response has been received from the tether host, TetherConnector connects to
// the associated Wi-Fi network.
class TetherConnector {
 public:
  using StringErrorCallback =
      NetworkConnectionHandler::TetherDelegate::StringErrorCallback;

  TetherConnector() {}

  TetherConnector(const TetherConnector&) = delete;
  TetherConnector& operator=(const TetherConnector&) = delete;

  virtual ~TetherConnector() {}

  virtual void ConnectToNetwork(const std::string& tether_network_guid,
                                base::OnceClosure success_callback,
                                StringErrorCallback error_callback) = 0;

  // Returns whether the connection attempt was successfully canceled.
  virtual bool CancelConnectionAttempt(
      const std::string& tether_network_guid) = 0;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_CONNECTOR_H_
