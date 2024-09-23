// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_CONNECTIONS_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_CONNECTIONS_MANAGER_H_

#include "base/no_destructor.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager_impl.h"

namespace ash::nearby::presence {

// `NearbyPresenceConnectionsManager` registers Nearby Presence's local
// `PresenceDeviceProvider` with  Nearby Connections with `NearbyConnections`
// during initialization to be used during authentication in calls to
// `ConnectV3()`. `NearbyPresenceConnectionsManager` then handles all Nearby
// Presence related connection logic.
class NearbyPresenceConnectionsManager : public NearbyConnectionsManagerImpl {
 public:
  NearbyPresenceConnectionsManager(
      raw_ptr<ash::nearby::NearbyProcessManager> process_manager);
  ~NearbyPresenceConnectionsManager() override;
  NearbyPresenceConnectionsManager(const NearbyPresenceConnectionsManager&) =
      delete;
  NearbyPresenceConnectionsManager& operator=(
      const NearbyPresenceConnectionsManager&) = delete;
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_CONNECTIONS_MANAGER_H_
