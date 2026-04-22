// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_REPORTING_CHOICE_SERVICE_H_
#define COMPONENTS_METRICS_METRICS_REPORTING_CHOICE_SERVICE_H_

#include "base/memory/raw_ptr.h"

class PrefRegistrySimple;
class PrefService;

namespace metrics {

// Service that helps in managing the new three-level metrics consent state.
// TODO(crbug.com/483043192): This feature is still under development.
class MetricsReportingChoiceService {
 public:
  explicit MetricsReportingChoiceService(PrefService* local_state);

  MetricsReportingChoiceService(const MetricsReportingChoiceService&) = delete;
  MetricsReportingChoiceService& operator=(
      const MetricsReportingChoiceService&) = delete;

  ~MetricsReportingChoiceService();

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  raw_ptr<PrefService> local_state_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_REPORTING_CHOICE_SERVICE_H_
