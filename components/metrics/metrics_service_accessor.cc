// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_service_accessor.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "build/branding_buildflags.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/variations/hashing.h"

namespace metrics {
namespace {

bool g_force_official_enabled_test = false;

bool IsMetricsReportingEnabledForOfficialBuild(PrefService* pref_service) {
  return pref_service->GetBoolean(prefs::kMetricsReportingEnabled);
}

}  // namespace

// static
bool MetricsServiceAccessor::IsMetricsReportingEnabled(
    PrefService* pref_service) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return IsMetricsReportingEnabledForOfficialBuild(pref_service);
#else
  // In non-official builds, disable metrics reporting completely.
  return g_force_official_enabled_test
             ? IsMetricsReportingEnabledForOfficialBuild(pref_service)
             : false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

// static
bool MetricsServiceAccessor::RegisterSyntheticFieldTrial(
    MetricsService* metrics_service,
    base::StringPiece trial_name,
    base::StringPiece group_name) {
  return RegisterSyntheticFieldTrialWithNameAndGroupHash(
      metrics_service, variations::HashName(trial_name),
      variations::HashName(group_name));
}

// static
bool MetricsServiceAccessor::RegisterSyntheticMultiGroupFieldTrial(
    MetricsService* metrics_service,
    base::StringPiece trial_name,
    const std::vector<uint32_t>& group_name_hashes) {
  if (!metrics_service)
    return false;

  metrics_service->synthetic_trial_registry()
      ->RegisterSyntheticMultiGroupFieldTrial(variations::HashName(trial_name),
                                              group_name_hashes);
  return true;
}

// static
bool MetricsServiceAccessor::RegisterSyntheticFieldTrialWithNameHash(
    MetricsService* metrics_service,
    uint32_t trial_name_hash,
    base::StringPiece group_name) {
  return RegisterSyntheticFieldTrialWithNameAndGroupHash(
      metrics_service, trial_name_hash, variations::HashName(group_name));
}

// static
bool MetricsServiceAccessor::RegisterSyntheticFieldTrialWithNameAndGroupHash(
    MetricsService* metrics_service,
    uint32_t trial_name_hash,
    uint32_t group_name_hash) {
  if (!metrics_service)
    return false;

  variations::SyntheticTrialGroup trial_group(trial_name_hash, group_name_hash);
  metrics_service->synthetic_trial_registry()->RegisterSyntheticFieldTrial(
      trial_group);
  return true;
}

// static
void MetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
    bool value) {
  g_force_official_enabled_test = value;
}

}  // namespace metrics
