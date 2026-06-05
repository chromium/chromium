// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_FEATURES_H_
#define COMPONENTS_SKILLS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

BASE_DECLARE_FEATURE(kSkillsEnabled);
BASE_DECLARE_FEATURE(kSkillsMetricsProviderEnabled);
BASE_DECLARE_FEATURE(kSkillsRefinementEnabled);
BASE_DECLARE_FEATURE(kSkillsAutocomplete);
BASE_DECLARE_FEATURE(kSkills1PDisabledForNonEnLocales);
BASE_DECLARE_FEATURE(kSkillsSubheadersEnabled);
BASE_DECLARE_FEATURE(kSkillsWebViewV2Enabled);

BASE_DECLARE_FEATURE(kSkillsServiceApi);
extern const base::FeatureParam<std::string> kSkillsServiceApiUrl;
extern const base::FeatureParam<std::string> kSkillsServiceApiOAuth2Scope;

}  // namespace features

#endif  // COMPONENTS_SKILLS_FEATURES_H_
