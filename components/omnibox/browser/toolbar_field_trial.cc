// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/toolbar_field_trial.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/base/ui_base_features.h"

namespace toolbar {
namespace features {

const base::Feature kHideSteadyStateUrlScheme {
  "OmniboxUIExperimentHideSteadyStateUrlScheme",
#if defined(OS_IOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

const base::Feature kHideSteadyStateUrlTrivialSubdomains {
  "OmniboxUIExperimentHideSteadyStateUrlTrivialSubdomains",
#if defined(OS_IOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

const base::Feature kHideFileUrlScheme{"OmniboxUIExperimentHideFileUrlScheme",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kHideSteadyStateUrlPathQueryAndRef {
  "OmniboxUIExperimentHideSteadyStateUrlPathQueryAndRef",
#if defined(OS_IOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

bool IsHideSteadyStateUrlSchemeEnabled() {
  return base::FeatureList::IsEnabled(kHideSteadyStateUrlScheme) ||
         base::FeatureList::IsEnabled(::features::kExperimentalUi);
}

bool IsHideSteadyStateUrlTrivialSubdomainsEnabled() {
  return base::FeatureList::IsEnabled(kHideSteadyStateUrlTrivialSubdomains) ||
         base::FeatureList::IsEnabled(::features::kExperimentalUi);
}

// Features used for EV UI removal experiment (https://crbug.com/803501).
const base::Feature kSimplifyHttpsIndicator{"SimplifyHttpsIndicator",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const char kSimplifyHttpsIndicatorParameterName[] = "treatment";
const char kSimplifyHttpsIndicatorParameterEvToSecure[] = "ev-to-secure";
const char kSimplifyHttpsIndicatorParameterSecureToLock[] = "secure-to-lock";
const char kSimplifyHttpsIndicatorParameterBothToLock[] = "both-to-lock";
const char kSimplifyHttpsIndicatorParameterKeepSecureChip[] =
    "keep-secure-chip";

}  // namespace features
}  // namespace toolbar
