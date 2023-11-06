// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/performance_controls_hats_service.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"

PerformanceControlsHatsService::PerformanceControlsHatsService(Profile* profile)
    : profile_(profile) {
  if (base::FeatureList::IsEnabled(
          performance_manager::features::
              kPerformanceControlsHighEfficiencyOptOutSurvey)) {
    performance_manager::user_tuning::UserPerformanceTuningManager::
        GetInstance()
            ->AddObserver(this);
  }

  if (base::FeatureList::IsEnabled(
          performance_manager::features::
              kPerformanceControlsBatterySaverOptOutSurvey)) {
    auto* manager = performance_manager::user_tuning::BatterySaverModeManager::
        GetInstance();
    battery_saver_observer_.Observe(manager);
  }
}

PerformanceControlsHatsService::~PerformanceControlsHatsService() {
  local_pref_registrar_.RemoveAll();

  // Can't used ScopedObservation because sometimes the
  // UserPerformanceTuningManager is destroyed before this service.
  if (performance_manager::user_tuning::UserPerformanceTuningManager::
          HasInstance()) {
    performance_manager::user_tuning::UserPerformanceTuningManager::
        GetInstance()
            ->RemoveObserver(this);
  }
}

void PerformanceControlsHatsService::OpenedNewTabPage() {
  HatsService* hats_service = HatsServiceFactory::GetForProfile(profile_, true);
  if (!hats_service) {
    return;
  }

  const bool battery_saver_mode =
      performance_manager::user_tuning::BatterySaverModeManager::GetInstance()
          ->IsBatterySaverModeEnabled();
  const bool high_efficiency_mode =
      performance_manager::user_tuning::UserPerformanceTuningManager::
          GetInstance()
              ->IsHighEfficiencyModeActive();

  // A general performance survey for all users.
  if (base::FeatureList::IsEnabled(performance_manager::features::
                                       kPerformanceControlsPerformanceSurvey)) {
    hats_service->LaunchSurvey(kHatsSurveyTriggerPerformanceControlsPerformance,
                               base::DoNothing(), base::DoNothing(),
                               {{"high_efficiency_mode", high_efficiency_mode},
                                {"battery_saver_mode", battery_saver_mode}},
                               {});
  }

  base::Time last_battery_timestamp =
      performance_manager::user_tuning::BatterySaverModeManager::GetInstance()
          ->GetLastBatteryUsageTimestamp();

  // A battery performance survey for users with a battery-powered device.
  if (base::FeatureList::IsEnabled(
          performance_manager::features::
              kPerformanceControlsBatteryPerformanceSurvey) &&
      (base::Time::Now() - last_battery_timestamp) <=
          performance_manager::features::
              kPerformanceControlsBatterySurveyLookback.Get()) {
    hats_service->LaunchSurvey(
        kHatsSurveyTriggerPerformanceControlsBatteryPerformance,
        base::DoNothing(), base::DoNothing(),
        {{"high_efficiency_mode", high_efficiency_mode},
         {"battery_saver_mode", battery_saver_mode}},
        {});
  }
}

void PerformanceControlsHatsService::OnBatterySaverModeChanged(
    bool is_enabled) {
  HatsService* hats_service = HatsServiceFactory::GetForProfile(profile_, true);
  if (!hats_service) {
    return;
  }

  auto* manager =
      performance_manager::user_tuning::BatterySaverModeManager::GetInstance();
  // A survey for users who have turned off battery saver.
  if (!is_enabled && !manager->IsBatterySaverModeManaged()) {
    hats_service->LaunchDelayedSurvey(
        kHatsSurveyTriggerPerformanceControlsBatterySaverOptOut, 10000);
  }
}

void PerformanceControlsHatsService::OnHighEfficiencyModeChanged() {
  HatsService* hats_service = HatsServiceFactory::GetForProfile(profile_, true);
  if (!hats_service) {
    return;
  }

  auto* manager = performance_manager::user_tuning::
      UserPerformanceTuningManager::GetInstance();
  // A survey for users who have turned off high efficiency mode.
  if (!manager->IsHighEfficiencyModeActive() &&
      !manager->IsHighEfficiencyModeManaged() &&
      !manager->IsHighEfficiencyModeDefault()) {
    hats_service->LaunchDelayedSurvey(
        kHatsSurveyTriggerPerformanceControlsHighEfficiencyOptOut, 10000);
  }
}
