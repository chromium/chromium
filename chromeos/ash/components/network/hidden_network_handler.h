// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_HIDDEN_NETWORK_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_HIDDEN_NETWORK_HANDLER_H_

#include "base/component_export.h"

namespace ash {

class NetworkStateHandler;
class NetworkConfigurationHandler;

// This class is responsible for removing wrongly hidden networks by
// performing network updates daily using a timer. Networks are
// considered to be wrongly hidden if:
// - Must have never been connected to.
// - Must have existed for >= 2 weeks.
// - Must not be a managed network.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) HiddenNetworkHandler {
 public:
  HiddenNetworkHandler();
  HiddenNetworkHandler(const HiddenNetworkHandler&) = delete;
  HiddenNetworkHandler& operator=(const HiddenNetworkHandler&) = delete;
  ~HiddenNetworkHandler() = default;

  void Init(NetworkStateHandler* network_state_handler,
            NetworkConfigurationHandler* network_configuration_handler);

 private:
  NetworkStateHandler* network_state_handler_;
  NetworkConfigurationHandler* network_configuration_handler_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_HIDDEN_NETWORK_HANDLER_H_
