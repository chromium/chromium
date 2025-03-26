// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/performance_controls_hats_service.h"

#include <algorithm>
#include <string>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "components/performance_manager/public/features.h"

using performance_manager::features::kPerformanceControlsPPMSurvey;
using performance_manager::features::kPerformanceControlsPPMSurveyMaxDelay;
using performance_manager::features::kPerformanceControlsPPMSurveyMinDelay;

PerformanceControlsHatsService::PerformanceControlsHatsService(Profile* profile)
    : profile_(profile),
      delay_before_ppm_survey_(
          base::RandTimeDelta(kPerformanceControlsPPMSurveyMinDelay.Get(),
                              kPerformanceControlsPPMSurveyMaxDelay.Get())) {
  CHECK(delay_before_ppm_survey_.is_positive());
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

  auto launch_survey_if_enabled =
      [hats_service, battery_saver_mode, memory_saver_mode](
          const base::Feature& feature, const std::string& trigger) {
        if (base::FeatureList::IsEnabled(feature)) {
          hats_service->LaunchSurvey(
              trigger, base::DoNothing(), base::DoNothing(),
              {{"Memory Saver Mode Enabled", memory_saver_mode},
               {"Battery Saver Mode Enabled", battery_saver_mode}},
              {});
        }
      };

  // A general performance survey for all users.
  launch_survey_if_enabled(
      performance_manager::features::kPerformanceControlsPerformanceSurvey,
      kHatsSurveyTriggerPerformanceControlsPerformance);

  // Survey to correlate UMA metrics with Poor Performance Moments.
  if (MayLaunchPPMSurvey()) {
    launch_survey_if_enabled(kPerformanceControlsPPMSurvey,
                             kHatsSurveyTriggerPerformanceControlsPPM);
  }

#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS defaults to the OS battery saver so this survey isn't relevant.
#else
  base::Time last_battery_timestamp =
      performance_manager::user_tuning::BatterySaverModeManager::GetInstance()
          ->GetLastBatteryUsageTimestamp();

  // A battery performance survey for users with a battery-powered device.
  if (base::Time::Now() - last_battery_timestamp <=
      performance_manager::features::kPerformanceControlsBatterySurveyLookback
          .Get()) {
    launch_survey_if_enabled(
        performance_manager::features::
            kPerformanceControlsBatteryPerformanceSurvey,
        kHatsSurveyTriggerPerformanceControlsBatteryPerformance);
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

bool PerformanceControlsHatsService::MayLaunchPPMSurvey() const {
  base::TimeDelta current_delay = base::Time::Now() - profile_->GetStartTime();
  if (current_delay < delay_before_ppm_survey_) {
    return false;
  }
  // Allow an extra minute in case the random delay is right at the end of the
  // range.
  base::TimeDelta max_delay =
      std::max(kPerformanceControlsPPMSurveyMaxDelay.Get(),
               delay_before_ppm_survey_ + base::Minutes(1));
  if (current_delay > max_delay) {
    return false;
  }
  return true;
}
