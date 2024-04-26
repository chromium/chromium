// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manual_fallback_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"

namespace password_manager {

namespace {
constexpr char kShowSuggestionLatency[] =
    "PasswordManager.ManualFallback.ShowSuggestions.Latency";
}  // namespace

PasswordManualFallbackMetricsRecorder::PasswordManualFallbackMetricsRecorder() =
    default;
PasswordManualFallbackMetricsRecorder::PasswordManualFallbackMetricsRecorder(
    const PasswordManualFallbackMetricsRecorder&) = default;
PasswordManualFallbackMetricsRecorder&
PasswordManualFallbackMetricsRecorder::operator=(
    const PasswordManualFallbackMetricsRecorder&) = default;
PasswordManualFallbackMetricsRecorder::
    ~PasswordManualFallbackMetricsRecorder() = default;

void PasswordManualFallbackMetricsRecorder::RecordDataFetchingLatency() const {
  base::TimeDelta duration = base::Time::Now() - start_;

  base::UmaHistogramTimes(kShowSuggestionLatency, duration);
}

}  // namespace password_manager
