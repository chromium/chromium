// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "ui/base/ui_base_features.h"

namespace ntp_tiles {

const char kPopularSitesFieldTrialName[] = "NTPPopularSites";

const char kIOSPopularSitesExcludePopularAppsParam[] =
    "IOSPopularSitesExcludePopularAppsParam";

BASE_FEATURE(kPopularSitesBakedInContentFeature,
             "NTPPopularSitesBakedInContent",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNtpMostLikelyFaviconsFromServerFeature,
             "NTPMostLikelyFaviconsFromServer",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUsePopularSitesSuggestions,
             "UsePopularSitesSuggestions",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSPopularSitesImprovedSuggestions,
             "IOSPopularSitesImprovedSuggestions",
             base::FEATURE_DISABLED_BY_DEFAULT);

IOSDefaultPopularSitesExperimentBehavior
GetDefaultPopularSitesExperimentType() {
  if (base::FeatureList::IsEnabled(kIOSPopularSitesImprovedSuggestions)) {
    return base::GetFieldTrialParamByFeatureAsBool(
               kIOSPopularSitesImprovedSuggestions,
               kIOSPopularSitesExcludePopularAppsParam, /*default_value=*/false)
               ? IOSDefaultPopularSitesExperimentBehavior::kExcludePopularApps
               : IOSDefaultPopularSitesExperimentBehavior::kIncludePopularApps;
  }

  return IOSDefaultPopularSitesExperimentBehavior::kDefault;
}

}  // namespace ntp_tiles
