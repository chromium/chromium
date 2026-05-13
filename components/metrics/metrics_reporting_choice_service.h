// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_REPORTING_CHOICE_SERVICE_H_
#define COMPONENTS_METRICS_METRICS_REPORTING_CHOICE_SERVICE_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/metrics/metrics_reporting_level.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class PrefService;

namespace variations {
class SyntheticTrialRegistry;
}

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

  // Initializes the synthetic field trial for the metrics consent restructure
  // feature and caches the current feature state to local state for the next
  // session.
  static void InitSyntheticFieldTrial(
      PrefService* local_state,
      variations::SyntheticTrialRegistry* synthetic_trial_registry);

  // Returns true if kMetricsReportingLevel is set to either kBasic or
  // kAdvanced, which means that basic metrics reporting is enabled.
  static bool IsBasicMetricsReportingEnabled(const PrefService* local_state);

  // Returns true if kMetricsReportingLevel is set to kAdvanced.
  static bool IsAdvancedMetricsReportingEnabled(const PrefService* local_state);

  // Returns true if the metrics consent restructure feature is enabled.
  static bool IsMetricsConsentRestructureFeatureEnabled(
      const PrefService* local_state);

  // Sets the metrics reporting level to |level|.
  static void SetMetricsReportingLevel(PrefService* local_state,
                                       MetricsReportingLevel level);

  // Returns true if the metrics consent restructure should be used. This is
  // different from IsMetricsConsentRestructureFeatureEnabled() in that it also
  // checks if the migration has been completed (kMetricsReportingMigrationDone
  // is true).
  static bool ShouldUseMetricsConsentRestructure(
      const PrefService* local_state);

  // Clears the static cached feature state. Used only for testing.
  static void ClearCachedFeatureStateForTesting();

  // Returns true if metrics reporting is disabled by policy.
  static bool IsMetricsReportingDisabledByPolicy(
      const PrefService* local_state);

  // Adds a callback to be notified when the metrics reporting level changes.
  base::CallbackListSubscription AddOnMetricsReportingLevelChangedCallback(
      base::RepeatingClosure callback);

 private:
  void OnReportingLevelPrefChanged();

  const raw_ptr<PrefService> local_state_;
  PrefChangeRegistrar pref_registrar_;
  base::RepeatingCallbackList<void()> callback_list_;

  friend class MetricsReportingChoiceServiceTest;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_REPORTING_CHOICE_SERVICE_H_
