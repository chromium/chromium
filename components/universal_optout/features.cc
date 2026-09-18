// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/universal_optout/features.h"

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_IOS)
#include "base/ios/ios_util.h"
#endif

namespace universal_optout::features {

BASE_FEATURE(kUniversalOptOut,
             "UniversalOptOut",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUniversalOptOutExtension, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUniversalOptOutSettings, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsUniversalOptOutEnabled() {
#if BUILDFLAG(IS_IOS)
  if (!base::ios::IsRunningOnOrLater(18, 4, 0)) {
    return false;
  }
  if (base::ios::IsRunningOnIOS27OrLater()) {
    return base::FeatureList::IsEnabled(kUniversalOptOut);
  }
  return base::FeatureList::IsEnabled(kUniversalOptOut) &&
         base::FeatureList::IsEnabled(kUniversalOptOutExtension);
#else
  return base::FeatureList::IsEnabled(kUniversalOptOut);
#endif
}

const base::FeatureParam<std::string> kTargetLocations{&kUniversalOptOut,
                                                       "target_locations", ""};

const base::FeatureParam<base::TimeDelta> kEligibilityWindow{
    &kUniversalOptOut, "eligibility_window", base::Days(30)};

const base::FeatureParam<base::TimeDelta> kTrailingEligibilityWindow{
    &kUniversalOptOut, "trailing_eligibility_window", base::Days(90)};

std::vector<std::string> GetTargetLocations() {
  return base::SplitString(kTargetLocations.Get(), ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

}  // namespace universal_optout::features
