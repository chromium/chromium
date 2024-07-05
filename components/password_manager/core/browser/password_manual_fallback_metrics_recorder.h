// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANUAL_FALLBACK_METRICS_RECORDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANUAL_FALLBACK_METRICS_RECORDER_H_

#include "base/time/time.h"

namespace password_manager {

// Encapsulates logic for logging manual fallback related metrics.
class PasswordManualFallbackMetricsRecorder {
 public:
  PasswordManualFallbackMetricsRecorder();
  PasswordManualFallbackMetricsRecorder(
      const PasswordManualFallbackMetricsRecorder&) = delete;
  PasswordManualFallbackMetricsRecorder& operator=(
      const PasswordManualFallbackMetricsRecorder&) = delete;
  ~PasswordManualFallbackMetricsRecorder();

  // Assigns the current time to `latency_duration_start_`, which is then used
  // inside `RecordDataFetchingLatency()` to calculate how much time has passed
  // between the start of the fetch and the end of the fetch.
  void DataFetchingStarted();

  // Records "PasswordManager.ManualFallback.ShowSuggestions.Latency" metric.
  void RecordDataFetchingLatency() const;

 private:
  base::Time latency_duration_start_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANUAL_FALLBACK_METRICS_RECORDER_H_
