// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "ui/base/ui_base_features.h"

namespace ntp_tiles {

BASE_FEATURE(kAimButtonRefactor, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kAimButtonRefactorArmParamFeature,
                   &kAimButtonRefactor,
                   kAimButtonRefactorArmParam,
                   static_cast<int>(AimButtonRefactorArm::kDisabled));

AimButtonRefactorArm GetAimButtonRefactorArm() {
  if (base::FeatureList::IsEnabled(kAimButtonRefactor)) {
    return static_cast<AimButtonRefactorArm>(
        kAimButtonRefactorArmParamFeature.Get());
  }
  return AimButtonRefactorArm::kDisabled;
}

const char kPopularSitesFieldTrialName[] = "NTPPopularSites";

BASE_FEATURE(kPopularSitesBakedInContentFeature,
             "NTPPopularSitesBakedInContent",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNtpMostLikelyFaviconsFromServerFeature,
             "NTPMostLikelyFaviconsFromServer",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace ntp_tiles
