// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_reporting_choice_service.h"

#include <optional>

#include "base/check.h"
#include "base/feature_list.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_profile_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace metrics {

// static
void MetricsReportingChoiceService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kAdvancedReportingEnabled, false);
  registry->RegisterBooleanPref(prefs::kAdvancedReportingProfileMigrationDone,
                                false);
}

// static
void MetricsReportingChoiceService::SetAdvancedReportingEnabled(
    PrefService* profile_prefs,
    bool enabled) {
  CHECK(profile_prefs);
  profile_prefs->SetBoolean(prefs::kAdvancedReportingEnabled, enabled);
}

// static
bool MetricsReportingChoiceService::IsAdvancedReportingEnabled(
    const PrefService* profile_prefs) {
  CHECK(profile_prefs);
  return profile_prefs->GetBoolean(prefs::kAdvancedReportingEnabled);
}

// static
bool MetricsReportingChoiceService::ShouldUseMetricsConsentRestructure() {
  return base::FeatureList::IsEnabled(
      features::kRestructureMetricsConsentSettings);
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
