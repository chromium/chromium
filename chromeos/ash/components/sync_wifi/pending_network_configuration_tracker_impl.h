// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_PENDING_NETWORK_CONFIGURATION_TRACKER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_PENDING_NETWORK_CONFIGURATION_TRACKER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chromeos/ash/components/sync_wifi/pending_network_configuration_tracker.h"
#include "components/prefs/pref_service.h"

class PrefRegistrySimple;
class PrefService;

namespace ash::sync_wifi {

// Keeps track of in flight updates to the local network stack and persists
// them to disk.
class PendingNetworkConfigurationTrackerImpl
    : public PendingNetworkConfigurationTracker {
 public:
  explicit PendingNetworkConfigurationTrackerImpl(PrefService* pref_service);

  PendingNetworkConfigurationTrackerImpl(
      const PendingNetworkConfigurationTrackerImpl&) = delete;
  PendingNetworkConfigurationTrackerImpl& operator=(
      const PendingNetworkConfigurationTrackerImpl&) = delete;

  ~PendingNetworkConfigurationTrackerImpl() override;

  // Registers preferences used by this class in the provided |registry|.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // sync_wifi::PendingNetworkConfigurationTracker::
  std::string TrackPendingUpdate(
      const NetworkIdentifier& id,
      const std::optional<sync_pb::WifiConfigurationSpecifics>& specifics)
      override;
  void MarkComplete(const std::string& change_guid,
                    const NetworkIdentifier& id) override;
  void IncrementCompletedAttempts(const std::string& change_guid,
                                  const NetworkIdentifier& id) override;
  std::vector<PendingNetworkConfigurationUpdate> GetPendingUpdates() override;
  std::optional<PendingNetworkConfigurationUpdate> GetPendingUpdate(
      const std::string& change_guid,
      const NetworkIdentifier& id) override;

 private:
  raw_ptr<PrefService> pref_service_;
  base::Value::Dict dict_;
};

}  // namespace ash::sync_wifi

#endif  // CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_PENDING_NETWORK_CONFIGURATION_TRACKER_IMPL_H_
