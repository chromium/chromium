// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_reporting_choice_service.h"

#include <optional>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_level.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/synthetic_trial_registry.h"

namespace metrics {

namespace {
// Cached state of the metrics consent restructure feature for the current
// session. This is set only once and remains immutable for the session.
std::optional<bool> g_session_feature_state;
}  // namespace

MetricsReportingChoiceService::MetricsReportingChoiceService(
    PrefService* local_state)
    : local_state_(local_state) {
  CHECK(local_state_);
  pref_registrar_.Init(local_state_);
  pref_registrar_.Add(
      prefs::kMetricsReportingLevel,
      base::BindRepeating(
          &MetricsReportingChoiceService::OnReportingLevelPrefChanged,
          base::Unretained(this)));
}

MetricsReportingChoiceService::~MetricsReportingChoiceService() = default;

// static
void MetricsReportingChoiceService::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kMetricsReportingLevel,
                                static_cast<int>(MetricsReportingLevel::kNone));
  registry->RegisterBooleanPref(prefs::kMetricsReportingMigrationDone, false);
  registry->RegisterBooleanPref(prefs::kMetricsConsentRestructureFeatureState,
                                false);
}

// static
void MetricsReportingChoiceService::InitSyntheticFieldTrial(
    PrefService* local_state,
    variations::SyntheticTrialRegistry* synthetic_trial_registry) {
  CHECK(local_state);
  const bool feature_enabled = base::FeatureList::IsEnabled(
      features::kRestructureMetricsConsentSettings);
  local_state->SetBoolean(prefs::kMetricsConsentRestructureFeatureState,
                          feature_enabled);
  // Register the synthetic field trial so we can evaluate the impact of the
  // feature using UMA data.
  CHECK(synthetic_trial_registry);
  const bool session_state =
      IsMetricsConsentRestructureFeatureEnabled(local_state);
  synthetic_trial_registry->RegisterSyntheticFieldTrial(
      variations::SyntheticTrialGroup(
          "RestructureMetricsConsent", session_state ? "Enabled" : "Disabled",
          variations::SyntheticTrialAnnotationMode::kCurrentLog));
}

base::CallbackListSubscription
MetricsReportingChoiceService::AddOnMetricsReportingLevelChangedCallback(
    base::RepeatingClosure callback) {
  return callback_list_.Add(std::move(callback));
}

// static
bool MetricsReportingChoiceService::IsMetricsConsentRestructureFeatureEnabled(
    const PrefService* local_state) {
  CHECK(local_state);
  // Note: Since FeatureList is always initialized after this code is run for
  // the first time, g_session_feature_state will always resolve the Finch flag
  // state from the previous session. The benefit of doing this is that the
  // kRestructureMetricsConsentSettings always stays resolvable and immutable
  // throughout a single session.
  if (!g_session_feature_state.has_value()) {
    g_session_feature_state =
        local_state->GetBoolean(prefs::kMetricsConsentRestructureFeatureState);
  }
  return g_session_feature_state.value();
}

// static
void MetricsReportingChoiceService::ClearCachedFeatureStateForTesting() {
  g_session_feature_state.reset();
}

// static
void MetricsReportingChoiceService::SetMetricsReportingLevel(
    PrefService* local_state,
    MetricsReportingLevel level) {
  CHECK(local_state);
  local_state->SetInteger(prefs::kMetricsReportingLevel,
                          static_cast<int>(level));
}

// static
bool MetricsReportingChoiceService::ShouldUseMetricsConsentRestructure(
    const PrefService* local_state) {
  CHECK(local_state);
  return IsMetricsConsentRestructureFeatureEnabled(local_state) &&
         local_state->GetBoolean(prefs::kMetricsReportingMigrationDone);
}

// static
bool MetricsReportingChoiceService::IsBasicMetricsReportingEnabled(
    const PrefService* local_state) {
  CHECK(local_state);
  if (ShouldUseMetricsConsentRestructure(local_state)) {
    MetricsReportingLevel level = static_cast<MetricsReportingLevel>(
        local_state->GetInteger(prefs::kMetricsReportingLevel));
    switch (level) {
      case MetricsReportingLevel::kBasic:
      case MetricsReportingLevel::kAdvanced:
        return true;
      default:
        return false;
    }
  }
  return local_state->GetBoolean(prefs::kMetricsReportingEnabled);
}

// static
bool MetricsReportingChoiceService::IsAdvancedMetricsReportingEnabled(
    const PrefService* local_state) {
  CHECK(local_state);
  MetricsReportingLevel level = static_cast<MetricsReportingLevel>(
      local_state->GetInteger(prefs::kMetricsReportingLevel));
  return level == MetricsReportingLevel::kAdvanced;
}

// static
bool MetricsReportingChoiceService::IsMetricsReportingDisabledByPolicy(
    const PrefService* local_state) {
  CHECK(local_state);
  if (ShouldUseMetricsConsentRestructure(local_state)) {
    return local_state->IsManagedPreference(prefs::kMetricsReportingLevel) &&
           !IsBasicMetricsReportingEnabled(local_state);
  }
  return local_state->IsManagedPreference(prefs::kMetricsReportingEnabled) &&
         !IsBasicMetricsReportingEnabled(local_state);
}

void MetricsReportingChoiceService::OnReportingLevelPrefChanged() {
  callback_list_.Notify();
}

}  // namespace metrics
