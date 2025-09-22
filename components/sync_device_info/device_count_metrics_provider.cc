// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_count_metrics_provider.h"

#include <algorithm>
#include <numeric>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "base/metrics/histogram_functions.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace syncer {

using DeviceType = DeviceInfo::FormFactor;

namespace {

// Returns the count for the given |form_factor|, or 0 if not found.
int GetCount(const absl::flat_hash_map<DeviceInfo::FormFactor, int>& counts,
             DeviceInfo::FormFactor form_factor) {
  auto it = counts.find(form_factor);
  return it != counts.end() ? it->second : 0;
}

void RecordDeviceCountMetric(const std::string& histogram_name, int count) {
  base::UmaHistogramSparse(histogram_name, std::min(count, 100));
}

}  // namespace

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
    absl::flat_hash_map<DeviceType, int> count_by_type =
        tracker->CountActiveDevicesByType();
    const int total_devices = std::accumulate(
        count_by_type.begin(), count_by_type.end(), 0,
        [](int sum, const auto& pair) { return sum + pair.second; });

    max_total = std::max(max_total, total_devices);
    max_desktop_count = std::max(
        max_desktop_count,
        GetCount(count_by_type, DeviceInfo::FormFactor::kDesktop));
    max_phone_count = std::max(
        max_phone_count, GetCount(count_by_type, DeviceInfo::FormFactor::kPhone));
    max_tablet_count = std::max(
        max_tablet_count,
        GetCount(count_by_type, DeviceInfo::FormFactor::kTablet));
  }

  RecordDeviceCountMetric("Sync.DeviceCount2", max_total);
  RecordDeviceCountMetric("Sync.DeviceCount2.Desktop", max_desktop_count);
  RecordDeviceCountMetric("Sync.DeviceCount2.Phone", max_phone_count);
  RecordDeviceCountMetric("Sync.DeviceCount2.Tablet", max_tablet_count);
}

}  // namespace syncer
