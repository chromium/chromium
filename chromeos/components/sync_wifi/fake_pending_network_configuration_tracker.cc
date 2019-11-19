// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/fake_pending_network_configuration_tracker.h"

#include "base/guid.h"
#include "base/stl_util.h"
#include "chromeos/components/sync_wifi/pending_network_configuration_update.h"

namespace chromeos {

namespace sync_wifi {

FakePendingNetworkConfigurationTracker::
    FakePendingNetworkConfigurationTracker() = default;

FakePendingNetworkConfigurationTracker::
    ~FakePendingNetworkConfigurationTracker() = default;

std::string FakePendingNetworkConfigurationTracker::TrackPendingUpdate(
    const NetworkIdentifier& id,
    const base::Optional<sync_pb::WifiConfigurationSpecificsData>& specifics) {
  std::string change_id = base::GenerateGUID();
  id_to_pending_update_map_.emplace(
      id, PendingNetworkConfigurationUpdate(id, change_id, specifics,
                                            /*completed_attempts=*/0));
  id_to_completed_attempts_map_[id] = 0;

  return change_id;
}

void FakePendingNetworkConfigurationTracker::MarkComplete(
    const std::string& change_guid,
    const NetworkIdentifier& id) {
  if (base::Contains(id_to_pending_update_map_, id) &&
      id_to_pending_update_map_.at(id).change_guid() == change_guid) {
    id_to_pending_update_map_.erase(id);
  }
}

void FakePendingNetworkConfigurationTracker::IncrementCompletedAttempts(
    const std::string& change_id,
    const NetworkIdentifier& id) {
  base::Optional<PendingNetworkConfigurationUpdate> existing_update =
      GetPendingUpdate(change_id, id);
  id_to_pending_update_map_.emplace(
      std::piecewise_construct, std::forward_as_tuple(id),
      std::forward_as_tuple(id, change_id, existing_update->specifics(),
                            existing_update->completed_attempts() + 1));
  id_to_completed_attempts_map_[id]++;
}

std::vector<PendingNetworkConfigurationUpdate>
FakePendingNetworkConfigurationTracker::GetPendingUpdates() {
  std::vector<PendingNetworkConfigurationUpdate> list;
  for (const auto& it : id_to_pending_update_map_)
    list.emplace_back(it.second);
  return list;
}

base::Optional<PendingNetworkConfigurationUpdate>
FakePendingNetworkConfigurationTracker::GetPendingUpdate(
    const std::string& change_guid,
    const NetworkIdentifier& id) {
  if (!base::Contains(id_to_pending_update_map_, id) ||
      id_to_pending_update_map_.at(id).change_guid() != change_guid) {
    return base::nullopt;
  }

  return id_to_pending_update_map_.at(id);
}

PendingNetworkConfigurationUpdate*
FakePendingNetworkConfigurationTracker::GetPendingUpdateById(
    const NetworkIdentifier& id) {
  if (!base::Contains(id_to_pending_update_map_, id))
    return nullptr;

  return &id_to_pending_update_map_.at(id);
}

int FakePendingNetworkConfigurationTracker::GetCompletedAttempts(
    const NetworkIdentifier& id) {
  DCHECK(id_to_completed_attempts_map_.count(id));
  return id_to_completed_attempts_map_[id];
}

}  // namespace sync_wifi

}  // namespace chromeos
