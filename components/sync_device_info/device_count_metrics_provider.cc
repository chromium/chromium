// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_count_metrics_provider.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace syncer {

DeviceCountMetricsProvider::DeviceCountMetricsProvider(
    const ProvideTrackersCallback& provide_trackers)
    : provide_trackers_(provide_trackers) {}

DeviceCountMetricsProvider::~DeviceCountMetricsProvider() {}

int DeviceCountMetricsProvider::MaxActiveDeviceCount() const {
  std::vector<const DeviceInfoTracker*> trackers;
  provide_trackers_.Run(&trackers);
  int max = 0;
  for (auto* tracker : trackers) {
    max = std::max(max, tracker->CountActiveDevices());
  }
  return max;
}

void DeviceCountMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  base::UmaHistogramSparse("Sync.DeviceCount2",
                           std::min(MaxActiveDeviceCount(), 100));
}

}  // namespace syncer
