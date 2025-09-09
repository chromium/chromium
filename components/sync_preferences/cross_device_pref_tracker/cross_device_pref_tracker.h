// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_H_
#define COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_preferences/cross_device_pref_tracker/timestamped_pref_value.h"

namespace sync_preferences {

// Abstract interface for a keyed service responsible for querying the values of
// some selected non-syncing Prefs across all of a user's syncing devices. It
// allows clients to observe how a particular non-syncing Pref value differs
// across Chrome platforms and form factors.
class CrossDevicePrefTracker : public KeyedService {
 public:
  // Observer interface for remote changes.
  class Observer : public base::CheckedObserver {
   public:
    // Called when `pref_name` is updated to `pref_value` on a remote device.
    virtual void OnRemotePrefChanged(
        std::string_view pref_name,
        const TimestampedPrefValue& pref_value,
        const syncer::DeviceInfo& remote_device_info) {}
  };

  // Defines criteria for querying devices.
  struct DeviceFilter {
    std::optional<syncer::DeviceInfo::OsType> os_type;
    std::optional<syncer::DeviceInfo::FormFactor> form_factor;
  };

  ~CrossDevicePrefTracker() override;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Retrieves all values for a tracked pref matching the filter, sorted in
  // descending order by timestamp (i.e., most recent first).
  virtual std::vector<TimestampedPrefValue> GetValues(
      std::string_view pref_name,
      const DeviceFilter& filter) const = 0;

  // Convenience wrapper to get the single most recent value.
  //
  // NOTE: In the case of a timestamp collision, we'll use the value of the
  // device that's most recently updated with the sync servers (via
  // `DeviceInfo::last_updated_timestamp`).
  virtual std::optional<TimestampedPrefValue> GetMostRecentValue(
      std::string_view pref_name,
      const DeviceFilter& filter) const = 0;

 protected:
  CrossDevicePrefTracker() = default;
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_H_
