// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/hats_service.h"

#include <stddef.h>

#include <iostream>

#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_thread.h"

namespace {
// which survey we're triggering
const char kHatsSurveyTrigger[] = "survey";

const char kHatsSurveyProbability[] = "probability";

const char kHatsSurveyEnSiteID[] = "en_site_id";

const char kHatsSurveyTriggerDefault[] = "test";

const double kHatsSurveyProbabilityDefault = 1;

const char kHatsSurveyEnSiteIDDefault[] = "z4cctguzopq5x2ftal6vdgjrui";

const char kHatsSurveyTriggerSatisfaction[] = "satisfaction";

HatsFinchConfig CreateHatsFinchConfig() {
  HatsFinchConfig config;
  config.trigger = base::FeatureParam<std::string>(
                       &features::kHappinessTrackingSurveysForDesktop,
                       kHatsSurveyTrigger, kHatsSurveyTriggerDefault)
                       .Get();

  config.probability =
      base::FeatureParam<double>(&features::kHappinessTrackingSurveysForDesktop,
                                 kHatsSurveyProbability,
                                 kHatsSurveyProbabilityDefault)
          .Get();

  config.site_ids.insert(
      std::make_pair("en", base::FeatureParam<std::string>(
                               &features::kHappinessTrackingSurveysForDesktop,
                               kHatsSurveyEnSiteID, kHatsSurveyEnSiteIDDefault)
                               .Get()));
  return config;
}

}  // namespace

HatsFinchConfig::HatsFinchConfig() = default;
HatsFinchConfig::~HatsFinchConfig() = default;
HatsFinchConfig::HatsFinchConfig(const HatsFinchConfig& other) = default;

HatsService::HatsService(Profile* profile)
    : profile_(profile), hats_finch_config_(CreateHatsFinchConfig()) {}

void HatsService::LaunchSatisfactionSurvey() {
  if (ShouldShowSurvey(kHatsSurveyTriggerSatisfaction)) {
    Browser* browser = chrome::FindBrowserWithActiveWindow();
    if (browser && browser->is_type_tabbed())
      browser->window()->ShowHatsBubbleFromAppMenuButton();
  }
}

bool HatsService::ShouldShowSurvey(const std::string& trigger) const {
  if ((hats_finch_config_.trigger == trigger ||
       hats_finch_config_.trigger == kHatsSurveyTriggerDefault) &&
      !launch_hats_) {
    if (base::RandDouble() < hats_finch_config_.probability) {
      // we only want to ever show hats once per profile.
      launch_hats_ = true;
      return true;
    }
    // TODO add pref checks to avoid too many surveys for a single profile
  }
  return false;
}

// static
bool HatsService::launch_hats_ = false;
