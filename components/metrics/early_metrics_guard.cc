// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/early_metrics_guard.h"

#include "base/check.h"

namespace metrics {
namespace {

bool g_early_metrics_recording_active = false;

}  // namespace

EarlyMetricsGuard::EarlyMetricsGuard() {
  DCHECK(!g_early_metrics_recording_active);
  g_early_metrics_recording_active = true;
}

EarlyMetricsGuard::~EarlyMetricsGuard() {
  DCHECK(g_early_metrics_recording_active);
  g_early_metrics_recording_active = false;
}

// static
bool EarlyMetricsGuard::IsEarlyMetricsRecordingActive() {
  return g_early_metrics_recording_active;
}

}  // namespace metrics
