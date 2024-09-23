// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/tab_group_sync_metrics_logger.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace tab_groups {
namespace {

// Thresholds for considering a tab group as active.
constexpr base::TimeDelta kOneDay = base::Days(1);
constexpr base::TimeDelta kSevenDays = base::Days(7);
constexpr base::TimeDelta kTwentyEightDays = base::Days(28);

void LogGroupCreated(DeviceType group_create_origin) {
  base::UmaHistogramEnumeration(
      "TabGroups.Sync.TabGroup.Created.GroupCreateOrigin", group_create_origin);
}

void LogGroupRemoved(DeviceType group_create_origin) {
  base::UmaHistogramEnumeration(
      "TabGroups.Sync.TabGroup.Removed.GroupCreateOrigin", group_create_origin);
}

void LogGroupOpened(DeviceType group_create_origin,
                    const SavedTabGroup* group,
                    OpeningSource opening_source) {
  CHECK(group);
  base::UmaHistogramEnumeration("TabGroups.Sync.TabGroup.Opened.Reason",
                                opening_source);

  bool user_initiated = opening_source == OpeningSource::kOpenedFromRevisitUi;
  if (!user_initiated) {
    return;
  }

  base::UmaHistogramBoolean("TabGroups.Sync.GroupOpenedByUser.HasTitle",
                            !group->title().empty());

  // Creation origin is recorded only if it was opened manually.
  base::UmaHistogramEnumeration(
      "TabGroups.Sync.TabGroup.Opened.GroupCreateOrigin", group_create_origin);
}

void LogGroupClosed(DeviceType group_create_origin,
                    const SavedTabGroup* group,
                    ClosingSource closing_source) {
  CHECK(group);
  base::UmaHistogramEnumeration("TabGroups.Sync.TabGroup.Closed.Reason",
                                closing_source);

  bool user_initiated = closing_source == ClosingSource::kClosedByUser ||
                        closing_source == ClosingSource::kDeletedByUser;
  if (!user_initiated) {
    return;
  }

  base::UmaHistogramBoolean("TabGroups.Sync.GroupClosedByUser.HasTitle",
                            !group->title().empty());

  // Creation origin is recorded only if it was closed manually.
  base::UmaHistogramEnumeration(
      "TabGroups.Sync.TabGroup.Closed.GroupCreateOrigin", group_create_origin);
}

void LogGroupVisualsChanged(DeviceType group_create_origin) {
  base::UmaHistogramEnumeration(
      "TabGroups.Sync.TabGroup.VisualsChanged.GroupCreateOrigin",
      group_create_origin);
}

void LogGroupTabsReordered(DeviceType group_create_origin) {
  base::UmaHistogramEnumeration(
      "TabGroups.Sync.TabGroup.TabsReordered.GroupCreateOrigin",
      group_create_origin);
}

void LogTabAdded(DeviceType group_create_origin) {
  base::UmaHistogramEnumeration(
      "TabGroups.Sync.TabGroup.TabAdded.GroupCreateOrigin",
      group_create_origin);
}

void LogTabNavigated(DeviceType group_create_origin,
                     DeviceType tab_create_origin) {
  base::UmaHistogramEnumeration(
      "TabGroups.Sync.TabGroup.TabNavigated.GroupCreateOrigin",
      group_create_origin);
  base::UmaHistogramEnumeration(
      "TabGroups.Sync.TabGroup.TabNavigated.TabCreateOrigin",
      tab_create_origin);
}

void LogTabRemoved(DeviceType group_create_origin,
                   DeviceType tab_create_origin) {
  base::UmaHistogramEnumeration(
      "TabGroups.Sync.TabGroup.TabRemoved.GroupCreateOrigin",
      group_create_origin);
  base::UmaHistogramEnumeration(
      "TabGroups.Sync.TabGroup.TabRemoved.TabCreateOrigin", tab_create_origin);
}

void LogTabSelected(DeviceType group_create_origin,
                    DeviceType tab_create_origin,
                    const SavedTabGroup* group,
                    const SavedTabGroupTab* tab) {
  CHECK(group);
  if (!tab) {
    return;
  }

  base::UmaHistogramEnumeration(
      "TabGroups.Sync.TabGroup.TabSelected.GroupCreateOrigin",
      group_create_origin);
  base::UmaHistogramEnumeration(
      "TabGroups.Sync.TabGroup.TabSelected.TabCreateOrigin", tab_create_origin);
}

void LogTabGroupUserInteracted(DeviceType group_create_origin,
                               const SavedTabGroup* group) {
  base::UmaHistogramEnumeration(
      "TabGroups.Sync.TabGroup.UserInteracted.GroupCreateOrigin",
      group_create_origin);
  base::UmaHistogramBoolean("TabGroups.Sync.TabGroup.UserInteracted.HasTitle",
                            !group->title().empty());
}

}  // namespace

TabGroupSyncMetricsLogger::TabGroupSyncMetricsLogger(
    syncer::DeviceInfoTracker* device_info_tracker)
    : device_info_tracker_(device_info_tracker) {}

TabGroupSyncMetricsLogger::~TabGroupSyncMetricsLogger() = default;

void TabGroupSyncMetricsLogger::LogEvent(const EventDetails& event_details,
                                         const SavedTabGroup* group,
                                         const SavedTabGroupTab* tab) {
  // Record creator origin related metrics first.
  CHECK(group);

  auto group_create_cache_guid = group->creator_cache_guid();
  auto tab_create_cache_guid = tab ? tab->creator_cache_guid() : std::nullopt;
  DeviceType group_create_origin =
      GetDeviceTypeFromCacheGuid(group_create_cache_guid);
  DeviceType tab_create_origin =
      GetDeviceTypeFromCacheGuid(tab_create_cache_guid);

  switch (event_details.event_type) {
    case TabGroupEvent::kTabGroupCreated:
      LogGroupCreated(group_create_origin);
      break;
    case TabGroupEvent::kTabGroupRemoved:
      LogGroupRemoved(group_create_origin);
      break;
    case TabGroupEvent::kTabGroupOpened:
      LogGroupOpened(group_create_origin, group,
                     event_details.opening_source.value());
      break;
    case TabGroupEvent::kTabGroupClosed:
      LogGroupClosed(group_create_origin, group,
                     event_details.closing_source.value());
      break;
    case TabGroupEvent::kTabGroupVisualsChanged:
      LogGroupVisualsChanged(group_create_origin);
      break;
    case TabGroupEvent::kTabGroupTabsReordered:
      LogGroupTabsReordered(group_create_origin);
      break;
    case TabGroupEvent::kTabAdded:
      LogTabAdded(group_create_origin);
      LogTabGroupUserInteracted(group_create_origin, group);
      break;
    case TabGroupEvent::kTabNavigated:
      LogTabNavigated(group_create_origin, tab_create_origin);
      break;
    case TabGroupEvent::kTabRemoved:
      LogTabRemoved(group_create_origin, tab_create_origin);
      LogTabGroupUserInteracted(group_create_origin, group);
      break;
    case TabGroupEvent::kTabSelected:
      LogTabSelected(group_create_origin, tab_create_origin, group, tab);
      LogTabGroupUserInteracted(group_create_origin, group);
      break;
  }
}

DeviceType TabGroupSyncMetricsLogger::GetDeviceTypeFromCacheGuid(
    const std::optional<std::string>& cache_guid) const {
  if (!cache_guid.has_value()) {
    return DeviceType::kUnknown;
  }

  bool is_local =
      device_info_tracker_->IsRecentLocalCacheGuid(cache_guid.value());
  if (is_local) {
    return DeviceType::kLocal;
  }

  auto* device_info = device_info_tracker_->GetDeviceInfo(cache_guid.value());
  if (!device_info) {
    return DeviceType::kUnknown;
  }

  return GetDeviceTypeFromDeviceInfo(*device_info);
}

DeviceType TabGroupSyncMetricsLogger::GetDeviceTypeFromDeviceInfo(
    const syncer::DeviceInfo& device_info) const {
  // Map OsType and FormFactor to DeviceType.
  switch (device_info.os_type()) {
    case syncer::DeviceInfo::OsType::kWindows:
      return DeviceType::kWindows;
    case syncer::DeviceInfo::OsType::kMac:
      return DeviceType::kMac;
    case syncer::DeviceInfo::OsType::kLinux:
      return DeviceType::kLinux;
    case syncer::DeviceInfo::OsType::kChromeOsAsh:
      [[fallthrough]];
    case syncer::DeviceInfo::OsType::kChromeOsLacros:
      return DeviceType::kChromeOS;
    case syncer::DeviceInfo::OsType::kAndroid:
      switch (device_info.form_factor()) {
        case syncer::DeviceInfo::FormFactor::kPhone:
          return DeviceType::kAndroidPhone;
        case syncer::DeviceInfo::FormFactor::kTablet:
          return DeviceType::kAndroidTablet;
        default:
          return DeviceType::kUnknown;
      }
    case syncer::DeviceInfo::OsType::kIOS:
      switch (device_info.form_factor()) {
        case syncer::DeviceInfo::FormFactor::kPhone:
          return DeviceType::kIOSPhone;
        case syncer::DeviceInfo::FormFactor::kTablet:
          return DeviceType::kIOSTablet;
        default:
          return DeviceType::kUnknown;
      }
    default:
      return DeviceType::kUnknown;
  }
}

void TabGroupSyncMetricsLogger::RecordMetricsOnStartup(
    const std::vector<SavedTabGroup>& saved_tab_groups,
    const std::vector<bool>& is_remote) {
  int total_group_count = saved_tab_groups.size();
  int open_group_count = 0;
  int closed_group_count = 0;
  int remote_group_count = 0;
  int active_group_count_1_day = 0;
  int active_group_count_7_day = 0;
  int active_group_count_28_day = 0;
  int remote_active_group_count_1_day = 0;
  int remote_active_group_count_7_day = 0;
  int remote_active_group_count_28_day = 0;

  const base::Time current_time = base::Time::Now();
  for (size_t i = 0; i < saved_tab_groups.size(); ++i) {
    const auto& group = saved_tab_groups[i];
    bool is_remote_group = is_remote[i];

    const base::TimeDelta tab_group_age =
        current_time - group.creation_time_windows_epoch_micros();
    const base::TimeDelta duration_since_last_user_interaction =
        current_time - group.last_user_interaction_time();

    // Skip metrics if something is wrong with the clocks.
    if (tab_group_age.is_negative() ||
        duration_since_last_user_interaction.is_negative()) {
      continue;
    }

    if (group.local_group_id().has_value()) {
      open_group_count++;
    } else {
      closed_group_count++;
    }

    if (is_remote_group) {
      remote_group_count++;
    }

    if (duration_since_last_user_interaction <= kOneDay) {
      active_group_count_1_day++;
      if (is_remote_group) {
        remote_active_group_count_1_day++;
      }
    }

    if (duration_since_last_user_interaction <= kSevenDays) {
      active_group_count_7_day++;
      if (is_remote_group) {
        remote_active_group_count_7_day++;
      }
    }

    if (duration_since_last_user_interaction <= kTwentyEightDays) {
      active_group_count_28_day++;
      if (is_remote_group) {
        remote_active_group_count_28_day++;
      }
    }

    base::UmaHistogramCounts1M("TabGroups.Sync.SavedTabGroupAge",
                               tab_group_age.InMinutes());
    base::UmaHistogramCounts1M(
        "TabGroups.Sync.TimeSinceLastUserInteractionWithGroup",
        duration_since_last_user_interaction.InMinutes());
    base::UmaHistogramCounts10000("TabGroups.Sync.SavedTabGroupTabCount",
                                  group.saved_tabs().size());

    for (const SavedTabGroupTab& tab : group.saved_tabs()) {
      const base::TimeDelta duration_since_tab_modification =
          current_time - tab.update_time_windows_epoch_micros();
      if (duration_since_tab_modification.is_negative()) {
        continue;
      }

      base::UmaHistogramCounts1M(
          "TabGroups.Sync.SavedTabGroupTabTimeSinceModification",
          duration_since_tab_modification.InMinutes());
    }
  }

  base::UmaHistogramCounts10000("TabGroups.Sync.TotalTabGroupCount",
                                total_group_count);
  base::UmaHistogramCounts10000("TabGroups.Sync.OpenTabGroupCount",
                                open_group_count);
  base::UmaHistogramCounts10000("TabGroups.Sync.ClosedTabGroupCount",
                                closed_group_count);
  base::UmaHistogramCounts10000("TabGroups.Sync.RemoteTabGroupCount",
                                remote_group_count);

  base::UmaHistogramCounts10000("TabGroups.Sync.ActiveTabGroupCount.1Day",
                                active_group_count_1_day);
  base::UmaHistogramCounts10000("TabGroups.Sync.RemoteActiveTabGroupCount.1Day",
                                remote_active_group_count_1_day);

  base::UmaHistogramCounts10000("TabGroups.Sync.ActiveTabGroupCount.7Day",
                                active_group_count_7_day);
  base::UmaHistogramCounts10000("TabGroups.Sync.RemoteActiveTabGroupCount.7Day",
                                remote_active_group_count_7_day);

  base::UmaHistogramCounts10000("TabGroups.Sync.ActiveTabGroupCount.28Day",
                                active_group_count_28_day);
  base::UmaHistogramCounts10000(
      "TabGroups.Sync.RemoteActiveTabGroupCount.28Day",
      remote_active_group_count_28_day);
}

void TabGroupSyncMetricsLogger::RecordTabGroupDeletionsOnStartup(
    size_t group_count) {
  base::UmaHistogramCounts10000("TabGroups.Sync.NumberOfGroupsDeletedOnStartup",
                                group_count);
}

}  // namespace tab_groups
