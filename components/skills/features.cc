// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/features.h"

namespace features {

BASE_FEATURE(kSkillsEnabled, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSkillsMetricsProviderEnabled, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSkillsRefinementEnabled, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSkillsAutocomplete, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSkills1PDisabledForNonEnLocales,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSkillsSubheadersEnabled, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSkillsServiceApi, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kSkillsServiceApiUrl{
    &kSkillsServiceApi, "endpoint",
    "https://chromeskills.pa.googleapis.com/v1/management/"
    "firstPartySkills"};
const base::FeatureParam<std::string> kSkillsServiceApiOAuth2Scope{
    &kSkillsServiceApi, "oauth2_scope",
    "https://www.googleapis.com/auth/chromeskills"};

}  // namespace features
