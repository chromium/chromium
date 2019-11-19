// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_UKM_DEMOGRAPHIC_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_UKM_DEMOGRAPHIC_METRICS_PROVIDER_H_

namespace ukm {
class Report;
}

namespace metrics {

// TODO(crbug/1015094): The UkmDemographicMetricsProvider interface is only
// needed to break the dependency cycle `views -> ukm -> demographic metrics
// provider -> sync -> policy -> bookmarks -> views` by removing the dependency
// on the demographic metrics provider target to build the ukm service. This
// interface should be removed once the dependency cycle is solved at the root.

// Interface of the provider of the synced user’s noised birth year and gender
// to the UKM metrics server. For more details, see the documentation of
// DemographicMetricsProvider at
// components/metrics/demographic_metrics_provider.h.
class UkmDemographicMetricsProvider {
 public:
  virtual ~UkmDemographicMetricsProvider() = default;

  // Provides the synced user's noised birth year and gender to the UKM metrics
  // report.
  virtual void ProvideSyncedUserNoisedBirthYearAndGenderToReport(
      ukm::Report* report) = 0;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_UKM_DEMOGRAPHIC_METRICS_PROVIDER_H_