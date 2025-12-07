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

struct MaxDeviceCounts {
  int total = 0;
  int desktop = 0;
  int phone = 0;
  int tablet = 0;
};

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

  MaxDeviceCounts max_counts;

  for (const DeviceInfoTracker* tracker : trackers) {
    absl::flat_hash_map<DeviceType, int> count_by_type =
        tracker->CountActiveDevicesByType();

    const int total_devices = std::accumulate(
        count_by_type.begin(), count_by_type.end(), 0,
        [](int sum, const auto& pair) { return sum + pair.second; });

    max_counts.total = std::max(max_counts.total, total_devices);
    max_counts.desktop =
        std::max(max_counts.desktop,
                 GetCount(count_by_type, DeviceInfo::FormFactor::kDesktop));
    max_counts.phone =
        std::max(max_counts.phone,
                 GetCount(count_by_type, DeviceInfo::FormFactor::kPhone));
    max_counts.tablet =
        std::max(max_counts.tablet,
                 GetCount(count_by_type, DeviceInfo::FormFactor::kTablet));
  }

  RecordDeviceCountMetric("Sync.DeviceCount2", max_counts.total);
  RecordDeviceCountMetric("Sync.DeviceCount2.Desktop", max_counts.desktop);
  RecordDeviceCountMetric("Sync.DeviceCount2.Phone", max_counts.phone);
  RecordDeviceCountMetric("Sync.DeviceCount2.Tablet", max_counts.tablet);
}

}  // namespace syncer
