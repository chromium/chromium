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

BASE_FEATURE(kNewTabPageRetention,
             "IOSNewTabPageRetention",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kNewTabPageRetentionName[] = "IOSNewTabPageRetention";

const char kNewTabPageRetentionParam[] = "NewTabPageRetentionParam";

NewTabPageRetentionExperimentBehavior GetNewTabPageRetentionExperimentType() {
  if (base::FeatureList::IsEnabled(kNewTabPageRetention)) {
    return (NewTabPageRetentionExperimentBehavior)
        base::GetFieldTrialParamByFeatureAsInt(kNewTabPageRetention,
                                               kNewTabPageRetentionParam,
                                               /*default_value=*/0);
  }

  return NewTabPageRetentionExperimentBehavior::kDefault;
}

}  // namespace ntp_tiles
