// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/android_metrics_provider.h"

#include "base/metrics/histogram_macros.h"
#include "base/system/sys_info.h"

namespace metrics {

AndroidMetricsProvider::AndroidMetricsProvider() = default;

AndroidMetricsProvider::~AndroidMetricsProvider() = default;

bool AndroidMetricsProvider::ProvideHistograms() {
  // Equivalent to UMA_HISTOGRAM_BOOLEAN with the stability flag set.
  UMA_STABILITY_HISTOGRAM_ENUMERATION(
      "MemoryAndroid.LowRamDevice", base::SysInfo::IsLowEndDevice() ? 1 : 0, 2);

  return true;
}

void AndroidMetricsProvider::ProvidePreviousSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  // The low-ram device status is unlikely to change between browser restarts.
  // Hence, it's safe and useful to attach this status to a previous session
  // log.
  ProvideHistograms();
}
}  // namespace metrics
