// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_HIDDEN_NETWORK_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_HIDDEN_NETWORK_HANDLER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"

namespace ash {

class ManagedNetworkConfigurationHandler;
class NetworkStateHandler;
class NetworkMetadataStore;

// This class is responsible for removing wrongly hidden networks by
// performing network updates daily using a timer. Networks are
// considered to be wrongly hidden if:
// - Must have never been connected to.
// - Must have existed for >= 2 weeks.
// - Must not be a managed network.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) HiddenNetworkHandler {
 public:
  HiddenNetworkHandler() = default;
  HiddenNetworkHandler(const HiddenNetworkHandler&) = delete;
  HiddenNetworkHandler& operator=(const HiddenNetworkHandler&) = delete;
  ~HiddenNetworkHandler() = default;

  void Init(
      ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
      NetworkStateHandler* network_state_handler);
  // This method will update the NetworkMetadataStore used when querying for
  // metadata about the network, and will result in immediately checking
  // for any hidden and wrongly configured networks.
  void SetNetworkMetadataStore(NetworkMetadataStore* network_metadata_store);

 private:
  void CleanHiddenNetworks();

  // Allows us to have an initial delay before the first time we check for
  // wrongly configured networks. This delay is important to ensure that
  // networks specific to a user are available during our initial check.
  base::OneShotTimer initial_delay_timer_;

  // Allows us to check for wrongly configured networks on a daily basis.
  base::RepeatingTimer daily_event_timer_;

  raw_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_ = nullptr;
  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;
  raw_ptr<NetworkMetadataStore, DanglingUntriaged> network_metadata_store_ =
      nullptr;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_HIDDEN_NETWORK_HANDLER_H_
