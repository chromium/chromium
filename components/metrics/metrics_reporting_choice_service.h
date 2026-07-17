// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_REPORTING_CHOICE_SERVICE_H_
#define COMPONENTS_METRICS_METRICS_REPORTING_CHOICE_SERVICE_H_
class PrefService;

namespace metrics {

// Service that helps in managing the new three-level metrics consent state.
// TODO(crbug.com/483043192): This feature is still under development.
class MetricsReportingChoiceService {
 public:
  MetricsReportingChoiceService() = delete;

  // Returns true if basic metrics reporting is enabled.
  static bool IsBasicMetricsReportingEnabled(const PrefService* local_state);

  // Returns true if the metrics consent restructure feature is enabled.
  static bool IsMetricsConsentRestructureFeatureEnabled();

  // Returns true if the metrics consent restructure should be used.
  static bool ShouldUseMetricsConsentRestructure();

  // Returns true if metrics reporting is disabled by policy.
  static bool IsMetricsReportingDisabledByPolicy(
      const PrefService* local_state);

 private:
  friend class MetricsReportingChoiceServiceTest;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_REPORTING_CHOICE_SERVICE_H_
