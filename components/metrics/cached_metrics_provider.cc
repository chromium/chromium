// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/cached_metrics_provider.h"

#include "components/metrics/early_metrics_guard.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

CachedMetricsProvider::CachedMetricsProvider() = default;

CachedMetricsProvider::~CachedMetricsProvider() = default;

void CachedMetricsProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile) {
  if (EarlyMetricsGuard::IsEarlyMetricsRecordingActive()) {
    const SystemProfileProto* cached =
        EarlyMetricsGuard::GetCachedSystemProfile();
    if (cached) {
      ProvideSystemProfileMetricsFromCache(*cached, system_profile);
    }
    return;
  }

  ProvideSystemProfileMetricsNormal(system_profile);
}

}  // namespace metrics
