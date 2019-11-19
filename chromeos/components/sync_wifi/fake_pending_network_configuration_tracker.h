// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SYNC_WIFI_FAKE_PENDING_NETWORK_CONFIGURATION_TRACKER_H_
#define CHROMEOS_COMPONENTS_SYNC_WIFI_FAKE_PENDING_NETWORK_CONFIGURATION_TRACKER_H_

#include <map>

#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "chromeos/components/sync_wifi/network_identifier.h"
#include "chromeos/components/sync_wifi/pending_network_configuration_tracker.h"

namespace chromeos {

namespace sync_wifi {

class FakePendingNetworkConfigurationTracker
    : public PendingNetworkConfigurationTracker {
 public:
  FakePendingNetworkConfigurationTracker();
  ~FakePendingNetworkConfigurationTracker() override;

  // sync_wifi::PendingNetworkConfigurationtracker::
  std::string TrackPendingUpdate(
      const NetworkIdentifier& id,
      const base::Optional<sync_pb::WifiConfigurationSpecificsData>& specifics)
      override;
  void MarkComplete(const std::string& change_guid,
                    const NetworkIdentifier& id) override;
  void IncrementCompletedAttempts(const std::string& change_id,
                                  const NetworkIdentifier& id) override;
  std::vector<PendingNetworkConfigurationUpdate> GetPendingUpdates() override;
  base::Optional<PendingNetworkConfigurationUpdate> GetPendingUpdate(
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

  DISALLOW_COPY_AND_ASSIGN(FakePendingNetworkConfigurationTracker);
};

}  // namespace sync_wifi

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SYNC_WIFI_FAKE_PENDING_NETWORK_CONFIGURATION_TRACKER_H_
