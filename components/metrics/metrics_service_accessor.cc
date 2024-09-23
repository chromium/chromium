// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_service_accessor.h"

#include <string_view>

#include "base/base_switches.h"
#include "build/branding_buildflags.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_switches.h"
#include "components/prefs/pref_service.h"
#include "components/variations/hashing.h"
#include "components/variations/synthetic_trial_registry.h"

namespace metrics {
namespace {

bool g_force_official_enabled_test = false;

bool IsMetricsReportingEnabledForOfficialBuild(PrefService* local_state) {
  return local_state->GetBoolean(prefs::kMetricsReportingEnabled);
}

}  // namespace

// static
bool MetricsServiceAccessor::IsMetricsReportingEnabled(
    PrefService* local_state) {
  if (IsMetricsReportingForceEnabled()) {
    LOG(WARNING) << "Metrics Reporting is force enabled, data will be sent to "
                    "servers. Should not be used for tests.";
    return true;
  }
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return IsMetricsReportingEnabledForOfficialBuild(local_state);
#else
  // In non-official builds, disable metrics reporting completely.
  return g_force_official_enabled_test
             ? IsMetricsReportingEnabledForOfficialBuild(local_state)
             : false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

// static
bool MetricsServiceAccessor::RegisterSyntheticFieldTrial(
    MetricsService* metrics_service,
    std::string_view trial_name,
    std::string_view group_name,
    variations::SyntheticTrialAnnotationMode annotation_mode) {
  if (!metrics_service)
    return false;

  variations::SyntheticTrialGroup trial_group(trial_name, group_name,
                                              annotation_mode);
  metrics_service->GetSyntheticTrialRegistry()->RegisterSyntheticFieldTrial(
      trial_group);
  return true;
}

// static
void MetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
    bool value) {
  g_force_official_enabled_test = value;
}

// static
bool MetricsServiceAccessor::IsForceMetricsReportingEnabledPrefLookup() {
  return g_force_official_enabled_test;
}

}  // namespace metrics
