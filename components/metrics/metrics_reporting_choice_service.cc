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
#include "components/prefs/pref_change_registrar.h"
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

MetricsReportingChoiceService::MetricsReportingChoiceService() = default;
MetricsReportingChoiceService::~MetricsReportingChoiceService() = default;

void MetricsReportingChoiceService::MonitorAdvancedReportingPref(
    PrefService* profile_prefs) {
  if (monitored_profile_prefs_.contains(profile_prefs)) {
    return;
  }

  auto pref_registrar = std::make_unique<PrefChangeRegistrar>();
  pref_registrar->Init(profile_prefs);
  pref_registrar->Add(
      prefs::kAdvancedReportingEnabled,
      base::BindRepeating(&MetricsReportingChoiceService::OnPrefChanged,
                          base::Unretained(this), profile_prefs));

  monitored_profile_prefs_[profile_prefs] = MonitoredProfileInfo{
      .registrar = std::move(pref_registrar),
      .is_enabled = IsAdvancedReportingEnabled(profile_prefs),
  };

  UpdateAdvancedReportingEnabledForAllProfilesState();
}

void MetricsReportingChoiceService::StopMonitoringAdvancedReportingPref(
    PrefService* profile_prefs) {
  auto it = monitored_profile_prefs_.find(profile_prefs);
  if (it == monitored_profile_prefs_.end()) {
    return;
  }
  monitored_profile_prefs_.erase(it);

  UpdateAdvancedReportingEnabledForAllProfilesState();
}

bool MetricsReportingChoiceService::IsAdvancedReportingEnabledForAllProfiles()
    const {
  return is_advanced_reporting_enabled_for_all_profiles_;
}

void MetricsReportingChoiceService::OnPrefChanged(PrefService* profile_prefs) {
  auto it = monitored_profile_prefs_.find(profile_prefs);
  if (it == monitored_profile_prefs_.end()) {
    return;
  }
  const bool was_enabled = it->second.is_enabled;
  const bool is_enabled = IsAdvancedReportingEnabled(profile_prefs);
  it->second.is_enabled = is_enabled;

  const bool reset_client_state = was_enabled && !is_enabled;
  UpdateAdvancedReportingEnabledForAllProfilesState(reset_client_state);
}

void MetricsReportingChoiceService::
    UpdateAdvancedReportingEnabledForAllProfilesState(bool reset_client_state) {
  bool all_enabled = true;
  if (monitored_profile_prefs_.empty()) {
    all_enabled = false;
  } else {
    for (const auto& kv : monitored_profile_prefs_) {
      if (!kv.second.is_enabled) {
        all_enabled = false;
        break;
      }
    }
  }

  if (all_enabled != is_advanced_reporting_enabled_for_all_profiles_ ||
      reset_client_state) {
    is_advanced_reporting_enabled_for_all_profiles_ = all_enabled;
    OnAdvancedReportingEnabledForAllProfilesChanged(all_enabled,
                                                    reset_client_state);
  }
}

}  // namespace metrics
