// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_IMPL_H_
#define COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_IMPL_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_preferences/cross_device_pref_tracker.h"

class PrefService;

namespace syncer {
class DeviceInfoSyncService;
}  // namespace syncer

namespace sync_preferences {

class CrossDevicePrefTrackerImpl : public CrossDevicePrefTracker,
                                   public syncer::DeviceInfoTracker::Observer {
 public:
  CrossDevicePrefTrackerImpl(
      PrefService* profile_pref_service,
      PrefService* local_pref_service,
      syncer::DeviceInfoSyncService* device_info_sync_service);
  ~CrossDevicePrefTrackerImpl() override;

  // `CrossDevicePrefTracker` overrides
  void AddObserver(CrossDevicePrefTracker::Observer* observer) override;
  void RemoveObserver(CrossDevicePrefTracker::Observer* observer) override;
  std::vector<CrossDevicePrefTracker::TimestampedPrefValue> GetValues(
      std::string_view pref_name,
      const DeviceFilter& filter) const override;
  std::optional<CrossDevicePrefTracker::TimestampedPrefValue>
  GetMostRecentValue(std::string_view pref_name,
                     const DeviceFilter& filter) const override;

  // `KeyedService` override
  void Shutdown() override;

  // `syncer::DeviceInfoTracker::Observer` overrides
  void OnDeviceInfoChange() override;

 private:
  // TODO(crbug.com/441330511): Implement service initialization and state
  // management (PrefService pointers, DeviceInfoSyncService pointer, Observers,
  // etc.)
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_IMPL_H_
