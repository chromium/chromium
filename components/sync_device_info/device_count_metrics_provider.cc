// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_count_metrics_provider.h"

#include <algorithm>
#include <map>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace syncer {

using DeviceType = sync_pb::SyncEnums::DeviceType;

DeviceCountMetricsProvider::DeviceCountMetricsProvider(
    const ProvideTrackersCallback& provide_trackers)
    : provide_trackers_(provide_trackers) {}

DeviceCountMetricsProvider::~DeviceCountMetricsProvider() = default;

void DeviceCountMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  std::vector<const DeviceInfoTracker*> trackers;
  provide_trackers_.Run(&trackers);
  int max_total = 0;
  int max_desktop_count = 0;
  int max_phone_count = 0;
  int max_tablet_count = 0;
  for (auto* tracker : trackers) {
    std::map<DeviceType, int> count_by_type =
        tracker->CountActiveDevicesByType();
    int total_devices = 0;
    int desktop_count = 0;
    int phone_count = 0;
    int tablet_count = 0;
    for (const auto& [device_type, count] : count_by_type) {
      total_devices += count;
      switch (device_type) {
        case sync_pb::SyncEnums_DeviceType_TYPE_CROS:
        case sync_pb::SyncEnums_DeviceType_TYPE_LINUX:
        case sync_pb::SyncEnums_DeviceType_TYPE_MAC:
        case sync_pb::SyncEnums_DeviceType_TYPE_WIN:
          desktop_count += count;
          break;
        case sync_pb::SyncEnums_DeviceType_TYPE_PHONE:
          phone_count += count;
          break;
        case sync_pb::SyncEnums_DeviceType_TYPE_TABLET:
          tablet_count += count;
          break;
        case sync_pb::SyncEnums_DeviceType_TYPE_OTHER:
        case sync_pb::SyncEnums_DeviceType_TYPE_UNSET:
          break;
      }
    }
    max_total = std::max(max_total, total_devices);
    max_desktop_count = std::max(max_desktop_count, desktop_count);
    max_phone_count = std::max(max_phone_count, phone_count);
    max_tablet_count = std::max(max_tablet_count, tablet_count);
  }

  base::UmaHistogramSparse("Sync.DeviceCount2", std::min(max_total, 100));
  base::UmaHistogramSparse("Sync.DeviceCount2.Desktop",
                           std::min(max_desktop_count, 100));
  base::UmaHistogramSparse("Sync.DeviceCount2.Phone",
                           std::min(max_phone_count, 100));
  base::UmaHistogramSparse("Sync.DeviceCount2.Tablet",
                           std::min(max_tablet_count, 100));
}

}  // namespace syncer
