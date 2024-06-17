// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/tab_group_sync_metrics_logger.h"

#include "base/metrics/histogram_functions.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace tab_groups {

TabGroupSyncMetricsLogger::TabGroupSyncMetricsLogger(
    syncer::DeviceInfoTracker* device_info_tracker)
    : device_info_tracker_(device_info_tracker) {}

TabGroupSyncMetricsLogger::~TabGroupSyncMetricsLogger() = default;

void TabGroupSyncMetricsLogger::LogEvent(
    TabGroupEvent event,
    const std::optional<std::string>& group_create_cache_guid,
    const std::optional<std::string>& tab_create_cache_guid) {}

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

}  // namespace tab_groups
