// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/test/test_metrics_provider.h"

#include "base/metrics/histogram_macros.h"

namespace metrics {

void TestMetricsProvider::Init() {
  init_called_ = true;
}

void TestMetricsProvider::OnRecordingDisabled() {
  on_recording_disabled_called_ = true;
}

bool TestMetricsProvider::HasPreviousSessionData() {
  has_initial_stability_metrics_called_ = true;
  return has_initial_stability_metrics_;
}

void TestMetricsProvider::ProvidePreviousSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  UMA_STABILITY_HISTOGRAM_ENUMERATION("TestMetricsProvider.Initial", 1, 2);
  provide_initial_stability_metrics_called_ = true;
  ProvideCurrentSessionData(nullptr);
}

void TestMetricsProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  UMA_STABILITY_HISTOGRAM_ENUMERATION("TestMetricsProvider.Regular", 1, 2);
  provide_stability_metrics_called_ = true;
}

void TestMetricsProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile_proto) {
  provide_system_profile_metrics_called_ = true;
}

}  // namespace metrics
