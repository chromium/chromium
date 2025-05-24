// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_METRICS_LOGGER_IMPL_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_METRICS_LOGGER_IMPL_H_

#include <deque>
#include <map>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/delayed_task_handle.h"
#include "base/time/default_clock.h"
#include "components/saved_tab_groups/public/tab_group_sync_metrics_logger.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/signin/public/base/consent_level.h"
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
class TabGroupSyncMetricsLoggerImpl : public TabGroupSyncMetricsLogger {
 public:
  explicit TabGroupSyncMetricsLoggerImpl(
      syncer::DeviceInfoTracker* device_info_tracker);
  ~TabGroupSyncMetricsLoggerImpl() override;

  // TabGroupSyncMetricsLogger overrides.
  void LogEvent(const EventDetails& event_details,
                const SavedTabGroup* group,
                const SavedTabGroupTab* tab) override;
  void RecordMetricsOnStartup(
      const std::vector<SavedTabGroup>& saved_tab_groups,
      const std::vector<bool>& is_remote) override;
  void RecordTabGroupDeletionsOnStartup(size_t group_count) override;
  void RecordMetricsOnSignin(const std::vector<SavedTabGroup>& saved_tab_groups,
                             signin::ConsentLevel consent_level) override;
  void RecordSavedTabGroupNavigation(const LocalTabID& id,
                                     const GURL& url,
                                     SavedTabGroupType type,
                                     bool is_post,
                                     bool was_redirected,
                                     ukm::SourceId source_id) override;

  // Returns the DeviceType based on the sync cache guid which can resolve to a
  // local device or a remote device with a specific OS and form factor. The
  // passed `cache_guid` argument can be a creator cache guid or last updater
  // cache guid, which is then used in conjunction with tab group metrics.
  DeviceType GetDeviceTypeFromCacheGuid(
      const std::optional<std::string>& cache_guid) const;

  // Returns the DeviceType based on the OS and form factor.
  DeviceType GetDeviceTypeFromDeviceInfo(
      const syncer::DeviceInfo& device_info) const;

  void SetClockForTesting(base::Clock* clock);

 private:
  // For resolving device information.
  raw_ptr<syncer::DeviceInfoTracker> device_info_tracker_;

  // Called to clean entries in `recent_navigations_` that are too old.
  void PruneRecentNavigationsThatAreTooOld();

  // Object representing recent main frame navigations.
  struct RecentNavigation {
    GURL url;
    base::Time timestamp;

    RecentNavigation(const GURL url, base::Time timestamp)
        : url(url), timestamp(timestamp) {}
  };

  using RecentNavigationMap =
      std::map<LocalTabID, std::unique_ptr<std::deque<RecentNavigation>>>;
  RecentNavigationMap recent_navigations_;

  raw_ptr<base::Clock> clock_ = base::DefaultClock::GetInstance();
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_METRICS_LOGGER_IMPL_H_
