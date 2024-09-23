// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_METRICS_LOGGER_H_
#define COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_METRICS_LOGGER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/saved_tab_groups/types.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace tab_groups {
class SavedTabGroup;
class SavedTabGroupTab;

// LINT.IfChange(DeviceType)
// Represents device types which can be a local device or a remote device.
// If it is a local device, only `kLocal` will be captured.
// If it is a remote device, the OS and form factor will be captured.
// Used for metrics only. Used in relation to a cache guid attribution which
// is mapped to a device type for metrics purposes. These values are persisted
// to logs. Entries should not be renumbered and numeric values should never be
// reused.
enum class DeviceType {
  kUnknown = 0,
  kLocal = 1,  // Local device on which the metrics is recorded.
  kWindows = 2,
  kMac = 3,
  kLinux = 4,
  kChromeOS = 5,
  kAndroidPhone = 6,
  kAndroidTablet = 7,
  kIOSPhone = 8,
  kIOSTablet = 9,
  kMaxValue = kIOSTablet
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:DeviceType)

// Class to record histograms for events related to tab group sync,
// capturing information about the originating device type and form factor.
class TabGroupSyncMetricsLogger {
 public:
  explicit TabGroupSyncMetricsLogger(
      syncer::DeviceInfoTracker* device_info_tracker);
  ~TabGroupSyncMetricsLogger();

  // Central method to log various tab group events.
  void LogEvent(const EventDetails& event_details,
                const SavedTabGroup* group,
                const SavedTabGroupTab* tab);

  // Returns the DeviceType based on the sync cache guid which can resolve to a
  // local device or a remote device with a specific OS and form factor. The
  // passed `cache_guid` argument can be a creator cache guid or last updater
  // cache guid, which is then used in conjunction with tab group metrics.
  DeviceType GetDeviceTypeFromCacheGuid(
      const std::optional<std::string>& cache_guid) const;

  // Returns the DeviceType based on the OS and form factor.
  DeviceType GetDeviceTypeFromDeviceInfo(
      const syncer::DeviceInfo& device_info) const;

  // Records metrics about the state of service such as the number of active,
  // inactive, open, closed, remote saved groups on startup. Recorded 10 seconds
  // after startup.
  void RecordMetricsOnStartup(
      const std::vector<SavedTabGroup>& saved_tab_groups,
      const std::vector<bool>& is_remote);

  // Records metrics about number of groups deleted on startup.
  void RecordTabGroupDeletionsOnStartup(size_t group_count);

 private:
  // For resolving device information.
  raw_ptr<syncer::DeviceInfoTracker> device_info_tracker_;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_METRICS_LOGGER_H_
