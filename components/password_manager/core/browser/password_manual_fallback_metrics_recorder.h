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
      const PasswordManualFallbackMetricsRecorder&);
  PasswordManualFallbackMetricsRecorder& operator=(
      const PasswordManualFallbackMetricsRecorder&);
  ~PasswordManualFallbackMetricsRecorder();

  // Records "PasswordManager.ManualFallback.ShowSuggestions.Latency" metric.
  void RecordDataFetchingLatency() const;

 private:
  base::Time start_ = base::Time::Now();
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANUAL_FALLBACK_METRICS_RECORDER_H_
