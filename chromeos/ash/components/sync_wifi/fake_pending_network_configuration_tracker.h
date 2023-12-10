// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_FAKE_PENDING_NETWORK_CONFIGURATION_TRACKER_H_
#define CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_FAKE_PENDING_NETWORK_CONFIGURATION_TRACKER_H_

#include <map>
#include <optional>

#include "chromeos/ash/components/sync_wifi/network_identifier.h"
#include "chromeos/ash/components/sync_wifi/pending_network_configuration_tracker.h"

namespace ash::sync_wifi {

class FakePendingNetworkConfigurationTracker
    : public PendingNetworkConfigurationTracker {
 public:
  FakePendingNetworkConfigurationTracker();

  FakePendingNetworkConfigurationTracker(
      const FakePendingNetworkConfigurationTracker&) = delete;
  FakePendingNetworkConfigurationTracker& operator=(
      const FakePendingNetworkConfigurationTracker&) = delete;

  ~FakePendingNetworkConfigurationTracker() override;

  // sync_wifi::PendingNetworkConfigurationtracker::
  std::string TrackPendingUpdate(
      const NetworkIdentifier& id,
      const std::optional<sync_pb::WifiConfigurationSpecifics>& specifics)
      override;
  void MarkComplete(const std::string& change_guid,
                    const NetworkIdentifier& id) override;
  void IncrementCompletedAttempts(const std::string& change_id,
                                  const NetworkIdentifier& id) override;
  std::vector<PendingNetworkConfigurationUpdate> GetPendingUpdates() override;
  std::optional<PendingNetworkConfigurationUpdate> GetPendingUpdate(
      const std::string& change_guid,
      const NetworkIdentifier& id) override;

  // Get the matching PendingNetworkConfigurationUpdate by for a given |id|.
  // This is needed because some tests don't have insight into what
  // change_guids are used.
  PendingNetworkConfigurationUpdate* GetPendingUpdateById(
      const NetworkIdentifier& id);

  // This includes ssids which have already been removed from the tracker.
  int GetCompletedAttempts(const NetworkIdentifier& id);

 private:
  std::map<NetworkIdentifier, PendingNetworkConfigurationUpdate>
      id_to_pending_update_map_;

  // This map is not cleared when MarkComplete is called to allow tests to
  // verify that the expected number of retries were performed before removal.
  std::map<NetworkIdentifier, int> id_to_completed_attempts_map_;
};

}  // namespace ash::sync_wifi

#endif  // CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_FAKE_PENDING_NETWORK_CONFIGURATION_TRACKER_H_
