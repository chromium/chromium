// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_EARLY_SAFE_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_EARLY_SAFE_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

namespace metrics {

// Marker interface for metrics providers that are safe to run during the
// early startup phase (e.g., in `StartupData::RecordCoreSystemProfile()`).
//
// Providers inheriting from this MUST strictly adhere to these rules:
// 1. MUST NOT perform blocking disk I/O (enforced by ScopedDisallowBlocking).
// 2. MUST NOT query late-initialized globals or UI-thread bound services.
// 3. MUST check `EarlyMetricsGuard::IsEarlyMetricsRecordingActive()` before
//    emitting cycle-dependent or 1-per-log lifecycle histograms to prevent
//    double counting.
// 4. MUST NOT spin up background threads or perform heavy, blocking
//    computations.
class EarlySafeMetricsProvider : public MetricsProvider {};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_EARLY_SAFE_METRICS_PROVIDER_H_
