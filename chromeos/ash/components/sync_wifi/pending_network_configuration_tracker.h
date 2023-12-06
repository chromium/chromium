// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_PENDING_NETWORK_CONFIGURATION_TRACKER_H_
#define CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_PENDING_NETWORK_CONFIGURATION_TRACKER_H_

#include <optional>
#include <string>

#include "chromeos/ash/components/sync_wifi/pending_network_configuration_update.h"
#include "components/sync/protocol/wifi_configuration_specifics.pb.h"

namespace ash::sync_wifi {

class NetworkIdentifier;

// Tracks updates to the local network stack while they are pending, including
// how many attempts have been executed.  Updates are stored persistently.
class PendingNetworkConfigurationTracker {
 public:
  PendingNetworkConfigurationTracker() = default;

  PendingNetworkConfigurationTracker(
      const PendingNetworkConfigurationTracker&) = delete;
  PendingNetworkConfigurationTracker& operator=(
      const PendingNetworkConfigurationTracker&) = delete;

  virtual ~PendingNetworkConfigurationTracker() = default;

  // Adds an update to the list of in flight changes.  |change_uuid| is a
  // unique identifier for each update, |id| is the identifier for the network
  // which is getting updated, and |specifics| should be nullopt if the network
  // is being deleted.  Returns the change_guid.
  virtual std::string TrackPendingUpdate(
      const NetworkIdentifier& id,
      const std::optional<sync_pb::WifiConfigurationSpecifics>& specifics) = 0;

  // Removes the given change from the list.
  virtual void MarkComplete(const std::string& change_guid,
                            const NetworkIdentifier& id) = 0;

  // Returns all pending updates.
  virtual std::vector<PendingNetworkConfigurationUpdate>
  GetPendingUpdates() = 0;

  // Returns the requested pending update, if it exists.
  virtual std::optional<PendingNetworkConfigurationUpdate> GetPendingUpdate(
      const std::string& change_guid,
      const NetworkIdentifier& id) = 0;

  // Increments the number of completed attempts for the given update.  Be sure
  // that the |change_guid| and |ssid| exist in the tracker before calling.
  virtual void IncrementCompletedAttempts(const std::string& change_guid,
                                          const NetworkIdentifier& id) = 0;
};

}  // namespace ash::sync_wifi

#endif  // CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_PENDING_NETWORK_CONFIGURATION_TRACKER_H_
