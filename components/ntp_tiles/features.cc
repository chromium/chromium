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

BASE_FEATURE(kPopularSitesBakedInContentFeature,
             "NTPPopularSitesBakedInContent",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNtpMostLikelyFaviconsFromServerFeature,
             "NTPMostLikelyFaviconsFromServer",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUsePopularSitesSuggestions,
             "UsePopularSitesSuggestions",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNewTabPageFieldTrial,
             "IOSNewTabPageFieldTrials",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kNewTabPageFieldTrialName[] = "IOSNewTabPageFieldTrials";

const char kNewTabPageFieldTrialParam[] = "NewTabPageFieldTrialParam";

NewTabPageFieldTrialExperimentBehavior GetNewTabPageFieldTrialExperimentType() {
  if (base::FeatureList::IsEnabled(kNewTabPageFieldTrial)) {
    return (NewTabPageFieldTrialExperimentBehavior)
        base::GetFieldTrialParamByFeatureAsInt(kNewTabPageFieldTrial,
                                               kNewTabPageFieldTrialParam,
                                               /*default_value=*/0);
  }

  return NewTabPageFieldTrialExperimentBehavior::kDefault;
}

}  // namespace ntp_tiles
