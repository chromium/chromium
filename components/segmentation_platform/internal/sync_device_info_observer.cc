// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/sync_device_info_observer.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/sync/base/features.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/device_info_util.h"

namespace segmentation_platform {

using OsType = syncer::DeviceInfo::OsType;

namespace {

constexpr int kDefaultActiveDaysThreshold = 14;

base::TimeDelta Age(base::Time last_update, base::Time now) {
  // Don't allow negative age for things somehow updated in the future.
  return std::max(base::TimeDelta(), now - last_update);
}

// Determines if a device with |last_update| timestamp should be considered
// active, given the current time.
bool IsActiveDevice(base::Time last_update, base::Time now) {
  base::TimeDelta active_days_threshold =
      base::Days(base::GetFieldTrialParamByFeatureAsInt(
          kSegmentationDeviceCountByOsType, "active_days_threshold",
          kDefaultActiveDaysThreshold));
  return Age(last_update, now) < active_days_threshold;
}

// Keep the following in sync with variants in
// //tools/metrics/histograms/metadata/segmentation_platform/histograms.xml.
const char* ConvertOsTypeToString(OsType os_type) {
  switch (os_type) {
    case OsType::kWindows:
      return "Windows";
    case OsType::kMac:
      return "Mac";
    case OsType::kLinux:
      return "Linux";
    case OsType::kIOS:
      return "iOS";
    case OsType::kAndroid:
      return "Android";
    case OsType::kChromeOsAsh:
      return "ChromeOsAsh";
    case OsType::kChromeOsLacros:
      return "ChromeOsLacros";
    case OsType::kFuchsia:
      return "Fuchsia";
    case OsType::kUnknown:
      return "Unknown";
  }
}

}  // namespace

BASE_FEATURE(kSegmentationDeviceCountByOsType,
             "SegmentationDeviceCountByOsType",
             base::FEATURE_ENABLED_BY_DEFAULT);

SyncDeviceInfoObserver::SyncDeviceInfoObserver(
    syncer::DeviceInfoTracker* device_info_tracker)
    : device_info_tracker_(device_info_tracker) {
  DCHECK(device_info_tracker_);
  device_info_tracker_->AddObserver(this);
}

SyncDeviceInfoObserver::~SyncDeviceInfoObserver() {
  device_info_tracker_->RemoveObserver(this);
}

// Count device by os types and record them in UMA only if not recorded yet.
void SyncDeviceInfoObserver::OnDeviceInfoChange() {
  if (!device_info_tracker_->IsSyncing() || device_info_recorded_)
    return;

  device_info_recorded_ = true;

  // Record device count by OS types.
  std::map<OsType, int> count_by_os_type = CountActiveDevicesByOsType();

  // Record UMA metrics of device counts by OS types.
  // Record 0 when there are no devices associated with one OS type.
  for (int os_type_idx = static_cast<int>(OsType::kUnknown);
       os_type_idx <= static_cast<int>(OsType::kFuchsia); ++os_type_idx) {
    OsType os_type = static_cast<OsType>(os_type_idx);
    int count = count_by_os_type[os_type];
    base::UmaHistogramSparse(
        base::StringPrintf("SegmentationPlatform.DeviceCountByOsType.%s",
                           ConvertOsTypeToString(os_type)),
        std::min(count, 100));
  }
}

std::map<OsType, int> SyncDeviceInfoObserver::CountActiveDevicesByOsType()
    const {
  std::map<OsType, int> count_by_os_type;
  const base::Time now = base::Time::Now();
  for (const auto& device_info : device_info_tracker_->GetAllDeviceInfo()) {
    if (!IsActiveDevice(device_info->last_updated_timestamp(), now))
      continue;

    auto os_type = device_info->os_type();
    count_by_os_type[os_type] += 1;
  }
  return count_by_os_type;
}

}  // namespace segmentation_platform
