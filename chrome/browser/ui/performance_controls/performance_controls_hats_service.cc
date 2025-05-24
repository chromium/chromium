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
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/performance_manager/public/features.h"

using performance_manager::features::kPerformanceControlsPPMSurvey;
using performance_manager::features::kPerformanceControlsPPMSurveyMaxDelay;
using performance_manager::features::kPerformanceControlsPPMSurveyMinDelay;
using performance_manager::features::
    kPerformanceControlsPPMSurveySegmentMaxMemoryGB1;
using performance_manager::features::
    kPerformanceControlsPPMSurveySegmentMaxMemoryGB2;
using performance_manager::features::kPerformanceControlsPPMSurveySegmentName1;
using performance_manager::features::kPerformanceControlsPPMSurveySegmentName2;
using performance_manager::features::kPerformanceControlsPPMSurveySegmentName3;
using performance_manager::features::
    kPerformanceControlsPPMSurveyUniformSampleValue;

PerformanceControlsHatsService::PerformanceControlsHatsService(Profile* profile)
    : profile_(profile),
      delay_before_ppm_survey_(
          base::RandTimeDelta(kPerformanceControlsPPMSurveyMinDelay.Get(),
                              kPerformanceControlsPPMSurveyMaxDelay.Get())) {
  CHECK(delay_before_ppm_survey_.is_positive());
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
          const base::Feature& feature, const std::string& trigger,
          const SurveyBitsData& extra_data = {},
          const SurveyStringData& string_data = {}) {
        if (base::FeatureList::IsEnabled(feature)) {
          SurveyBitsData bits_data = {
              {kMemorySaverPSDName, memory_saver_mode},
              {kBatterySaverPSDName, battery_saver_mode}};
          for (const auto& [key, value] : extra_data) {
            auto [_, inserted] = bits_data.try_emplace(key, value);
            CHECK(inserted);
          }
          hats_service->LaunchSurvey(trigger, base::DoNothing(),
                                     base::DoNothing(), bits_data, string_data);
        }
      };

  // Survey to correlate UMA metrics with Poor Performance Moments.
  if (auto ppm_segment_name = GetPPMSurveySegmentName();
      !ppm_segment_name.empty() && MayLaunchPPMSurvey()) {
    const std::string channel =
        chrome::GetChannelName(chrome::WithExtendedStable(false));
    launch_survey_if_enabled(
        kPerformanceControlsPPMSurvey, kHatsSurveyTriggerPerformanceControlsPPM,
        {{kUniformSamplePSDName,
          kPerformanceControlsPPMSurveyUniformSampleValue.Get()}},
        {{kPerformanceSegmentPSDName, ppm_segment_name},
         {kChannelPSDName, channel.empty() ? "stable" : channel}});
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

std::string PerformanceControlsHatsService::GetPPMSurveySegmentName() {
  uint64_t system_ram = memory_mb_for_testing_.value_or(
      base::SysInfo::AmountOfPhysicalMemoryMB());
  size_t max_memory1 = kPerformanceControlsPPMSurveySegmentMaxMemoryGB1.Get();
  size_t max_memory2 = kPerformanceControlsPPMSurveySegmentMaxMemoryGB2.Get();
  if (max_memory1 == 0 || system_ram <= max_memory1 * 1024) {
    // Segment 1 has no upper bound, or the system RAM is in its bounds.
    return kPerformanceControlsPPMSurveySegmentName1.Get();
  }
  if (max_memory2 == 0 || system_ram <= max_memory2 * 1024) {
    // Segment 2 has no upper bound, or the system RAM is in its bounds.
    return kPerformanceControlsPPMSurveySegmentName2.Get();
  }
  return kPerformanceControlsPPMSurveySegmentName3.Get();
}
