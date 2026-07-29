// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_REPORTING_CHOICE_SERVICE_H_
#define COMPONENTS_METRICS_METRICS_REPORTING_CHOICE_SERVICE_H_
class PrefService;
class PrefRegistrySimple;

#include <map>
#include <memory>

class PrefChangeRegistrar;

namespace metrics {

// Service that helps in managing the new three-level metrics consent state.
// TODO(crbug.com/483043192): This feature is still under development.
class MetricsReportingChoiceService {
 public:
  MetricsReportingChoiceService(const MetricsReportingChoiceService&) = delete;
  MetricsReportingChoiceService& operator=(
      const MetricsReportingChoiceService&) = delete;

  // Starts observing a profile's kAdvancedReportingEnabled pref.
  void MonitorAdvancedReportingPref(PrefService* profile_prefs);

  // Stops observing a profile's kAdvancedReportingEnabled pref.
  void StopMonitoringAdvancedReportingPref(PrefService* profile_prefs);

  // Returns true if all monitored profiles have kAdvancedReportingEnabled set
  // to true.
  bool IsAdvancedReportingEnabledForAllProfiles() const;

  // Registers profile-level preferences used by this service.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Sets the advanced metrics reporting choice.
  static void SetAdvancedReportingEnabled(PrefService* profile_prefs,
                                          bool enabled);

  // Gets the current advanced metrics reporting choice.
  static bool IsAdvancedReportingEnabled(const PrefService* profile_prefs);

  // Returns true if basic metrics reporting is enabled.
  static bool IsBasicMetricsReportingEnabled(const PrefService* local_state);

  // Returns true if the metrics consent restructure should be used.
  static bool ShouldUseMetricsConsentRestructure();

  // Returns true if metrics reporting is disabled by policy.
  static bool IsMetricsReportingDisabledByPolicy(
      const PrefService* local_state);

 protected:
  MetricsReportingChoiceService();
  virtual ~MetricsReportingChoiceService();

  // Called when the "enabled for all profiles" state changes.
  virtual void OnAdvancedReportingEnabledForAllProfilesChanged(
      bool enabled,
      bool reset_client_state) {}

 private:
  friend class MetricsReportingChoiceServiceTest;

  void OnPrefChanged(PrefService* profile_prefs);
  void UpdateAdvancedReportingEnabledForAllProfilesState(
      bool reset_client_state = false);

  struct MonitoredProfileInfo {
    std::unique_ptr<PrefChangeRegistrar> registrar;
    bool is_enabled = false;
  };
  std::map<PrefService*, MonitoredProfileInfo> monitored_profile_prefs_;

  // Cached value of IsAdvancedReportingEnabledForAllProfiles().
  bool is_advanced_reporting_enabled_for_all_profiles_ = false;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_REPORTING_CHOICE_SERVICE_H_
