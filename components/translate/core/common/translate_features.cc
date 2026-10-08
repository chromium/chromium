// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/common/translate_features.h"

#include "base/base_switches.h"
#include "base/command_line.h"

namespace translate {

BASE_FEATURE(kEnableTranslatePdf, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTranslateSimplifiedHindi, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTranslateLanguageSearchUI, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTranslateElementExperimentFeatures,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kTranslateElementExperimentFeaturesParam{
    &kTranslateElementExperimentFeatures, "ef", ""};

BASE_FEATURE(kTranslateElementRegionalization,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPartialTranslateUseOnePlatformApi,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTranslateTrigger, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsTranslateTriggerEnabled() {
  if (!base::FeatureList::IsEnabled(kTranslateTrigger)) {
    return false;
  }
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kEnableBenchmarking)) {
    return true;
  }
  return base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
      kTranslateTrigger.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE);
}

}  // namespace translate
