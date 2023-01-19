// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/performance_controls_hats_service.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"

PerformanceControlsHatsService::PerformanceControlsHatsService(Profile* profile)
    : profile_(profile) {}

void PerformanceControlsHatsService::OpenedNewTabPage() {
  HatsService* hats_service = HatsServiceFactory::GetForProfile(profile_, true);

  // If none of the features are enabled, return early.
  if (!hats_service) {
    return;
  }

  const bool show_performance_survey = base::FeatureList::IsEnabled(
      performance_manager::features::kPerformanceControlsPerformanceSurvey);
  const bool show_battery_survey = base::FeatureList::IsEnabled(
      performance_manager::features::
          kPerformanceControlsBatteryPerformanceSurvey);
  const bool show_high_efficiency_opt_out_survey = base::FeatureList::IsEnabled(
      performance_manager::features::
          kPerformanceControlsHighEfficiencyOptOutSurvey);
  const bool show_battery_saver_opt_out_survey = base::FeatureList::IsEnabled(
      performance_manager::features::
          kPerformanceControlsBatterySaverOptOutSurvey);

  PrefService* prefs = g_browser_process->local_state();
  const int battery_saver_mode = prefs->GetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState);
  const bool high_efficiency_mode = prefs->GetBoolean(
      performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled);

  // A general performance survey for all users.
  if (show_performance_survey) {
    hats_service->LaunchSurvey(
        kHatsSurveyTriggerPerformanceControlsPerformance, base::DoNothing(),
        base::DoNothing(), {{"high_efficiency_mode", high_efficiency_mode}},
        {{"battery_saver_mode", base::NumberToString(battery_saver_mode)}});
  }

  // A battery performance survey for users with a battery-powered device.
  if (show_battery_survey && performance_manager::user_tuning::
                                 UserPerformanceTuningManager::GetInstance()
                                     ->DeviceHasBattery()) {
    hats_service->LaunchSurvey(
        kHatsSurveyTriggerPerformanceControlsBatteryPerformance,
        base::DoNothing(), base::DoNothing(),
        {{"high_efficiency_mode", high_efficiency_mode}},
        {{"battery_saver_mode", base::NumberToString(battery_saver_mode)}});
  }

  // A survey for users who have turned off high efficiency mode.
  if (show_high_efficiency_opt_out_survey && !high_efficiency_mode &&
      base::FeatureList::IsEnabled(
          performance_manager::features::kHighEfficiencyModeAvailable)) {
    auto* pref = prefs->FindPreference(
        performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled);
    if (!pref->IsManaged() && !pref->IsDefaultValue()) {
      hats_service->LaunchSurvey(
          kHatsSurveyTriggerPerformanceControlsHighEfficiencyOptOut);
    }
  }

  // A survey for users who have turned off battery saver.
  if (show_battery_saver_opt_out_survey &&
      base::FeatureList::IsEnabled(
          performance_manager::features::kBatterySaverModeAvailable) &&
      battery_saver_mode ==
          static_cast<int>(performance_manager::user_tuning::prefs::
                               BatterySaverModeState::kDisabled)) {
    auto* pref = prefs->FindPreference(
        performance_manager::user_tuning::prefs::kBatterySaverModeState);
    if (!pref->IsManaged()) {
      hats_service->LaunchSurvey(
          kHatsSurveyTriggerPerformanceControlsBatterySaverOptOut);
    }
  }
}
