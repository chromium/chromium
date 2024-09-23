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

PerformanceControlsHatsService::PerformanceControlsHatsService(Profile* profile)
    : profile_(profile) {
  if (base::FeatureList::IsEnabled(
          performance_manager::features::
              kPerformanceControlsMemorySaverOptOutSurvey)) {
    performance_manager::user_tuning::UserPerformanceTuningManager::
        GetInstance()
            ->AddObserver(this);
  }

  if (base::FeatureList::IsEnabled(
          performance_manager::features::
              kPerformanceControlsBatterySaverOptOutSurvey)) {
    performance_manager::user_tuning::BatterySaverModeManager::GetInstance()
        ->AddObserver(this);
  }
}

PerformanceControlsHatsService::~PerformanceControlsHatsService() {
  // Can't used ScopedObservation because sometimes the
  // UserPerformanceTuningManager or BatterySaverModeManager are destroyed
  // before this service.
  if (performance_manager::user_tuning::UserPerformanceTuningManager::
          HasInstance()) {
    performance_manager::user_tuning::UserPerformanceTuningManager::
        GetInstance()
            ->RemoveObserver(this);
  }

  if (performance_manager::user_tuning::BatterySaverModeManager::
          HasInstance()) {
    performance_manager::user_tuning::BatterySaverModeManager::GetInstance()
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
  const bool memory_saver_mode = performance_manager::user_tuning::
                                     UserPerformanceTuningManager::GetInstance()
                                         ->IsMemorySaverModeActive();

  // A general performance survey for all users.
  if (base::FeatureList::IsEnabled(performance_manager::features::
                                       kPerformanceControlsPerformanceSurvey)) {
    hats_service->LaunchSurvey(kHatsSurveyTriggerPerformanceControlsPerformance,
                               base::DoNothing(), base::DoNothing(),
                               {{"high_efficiency_mode", memory_saver_mode},
                                {"battery_saver_mode", battery_saver_mode}},
                               {});
  }

// ChromeOS defaults to the OS battery saver so this survey isn't relevant.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
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
        {{"high_efficiency_mode", memory_saver_mode},
         {"battery_saver_mode", battery_saver_mode}},
        {});
  }

#endif
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

void PerformanceControlsHatsService::OnMemorySaverModeChanged() {
  HatsService* hats_service = HatsServiceFactory::GetForProfile(profile_, true);
  if (!hats_service) {
    return;
  }

  auto* manager = performance_manager::user_tuning::
      UserPerformanceTuningManager::GetInstance();
  // A survey for users who have turned off memory saver mode.
  if (!manager->IsMemorySaverModeActive() &&
      !manager->IsMemorySaverModeManaged() &&
      !manager->IsMemorySaverModeDefault()) {
    hats_service->LaunchDelayedSurvey(
        kHatsSurveyTriggerPerformanceControlsMemorySaverOptOut, 10000);
  }
}
