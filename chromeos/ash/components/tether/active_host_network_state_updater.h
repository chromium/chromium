// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_ACTIVE_HOST_NETWORK_STATE_UPDATER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_ACTIVE_HOST_NETWORK_STATE_UPDATER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/tether/active_host.h"

namespace ash {

class NetworkStateHandler;

namespace tether {

// Observes changes to the status of the active host, and relays these updates
// to the networking stack.
class ActiveHostNetworkStateUpdater final : public ActiveHost::Observer {
 public:
  ActiveHostNetworkStateUpdater(ActiveHost* active_host,
                                NetworkStateHandler* network_state_handler);

  ActiveHostNetworkStateUpdater(const ActiveHostNetworkStateUpdater&) = delete;
  ActiveHostNetworkStateUpdater& operator=(
      const ActiveHostNetworkStateUpdater&) = delete;

  ~ActiveHostNetworkStateUpdater();

  // ActiveHost::Observer:
  void OnActiveHostChanged(
      const ActiveHost::ActiveHostChangeInfo& change_info) override;

 private:
  raw_ptr<ActiveHost> active_host_;
  raw_ptr<NetworkStateHandler> network_state_handler_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_ACTIVE_HOST_NETWORK_STATE_UPDATER_H_
