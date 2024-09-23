// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/nearby_presence_connections_manager.h"

namespace {

const char kNearbyPresenceServiceId[] = "nearby_presence";

}  // namespace

namespace ash::nearby::presence {

NearbyPresenceConnectionsManager::NearbyPresenceConnectionsManager(
    raw_ptr<ash::nearby::NearbyProcessManager> process_manager)
    : NearbyConnectionsManagerImpl(process_manager, kNearbyPresenceServiceId) {
  GetNearbyConnections()->RegisterServiceWithPresenceDeviceProvider(
      service_id_);
}

NearbyPresenceConnectionsManager::~NearbyPresenceConnectionsManager() = default;

}  // namespace ash::nearby::presence
