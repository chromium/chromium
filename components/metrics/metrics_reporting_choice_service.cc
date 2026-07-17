// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_reporting_choice_service.h"

#include <optional>

#include "base/check.h"
#include "base/feature_list.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace metrics {

// static
bool MetricsReportingChoiceService::
    IsMetricsConsentRestructureFeatureEnabled() {
  return base::FeatureList::IsEnabled(
      features::kRestructureMetricsConsentSettings);
}

// static
bool MetricsReportingChoiceService::ShouldUseMetricsConsentRestructure() {
  return IsMetricsConsentRestructureFeatureEnabled();
}

// static
bool MetricsReportingChoiceService::IsBasicMetricsReportingEnabled(
    const PrefService* local_state) {
  CHECK(local_state);
  return local_state->GetBoolean(prefs::kMetricsReportingEnabled);
}

// static
bool MetricsReportingChoiceService::IsMetricsReportingDisabledByPolicy(
    const PrefService* local_state) {
  CHECK(local_state);
  return local_state->IsManagedPreference(prefs::kMetricsReportingEnabled) &&
         !IsBasicMetricsReportingEnabled(local_state);
}

}  // namespace metrics
