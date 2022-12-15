// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/common/language_experiments.h"

#include <map>
#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"

namespace language {
// Features:
BASE_FEATURE(kOverrideTranslateTriggerInIndia,
             "OverrideTranslateTriggerInIndia",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kExplicitLanguageAsk,
             "ExplicitLanguageAsk",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAppLanguagePrompt,
             "AppLanguagePrompt",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAppLanguagePromptULP,
             "AppLanguagePromptULP",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kForceAppLanguagePrompt,
             "ForceAppLanguagePrompt",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDetailedLanguageSettings,
             "DetailedLanguageSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDesktopDetailedLanguageSettings,
             "DesktopDetailedLanguageSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTranslateAssistContent,
             "TranslateAssistContent",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTranslateIntent,
             "TranslateIntent",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kContentLanguagesInLanguagePicker,
             "ContentLanguagesInLanguagePicker",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCctAutoTranslate,
             "CCTAutoTranslate",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Params:
const char kBackoffThresholdKey[] = "backoff_threshold";
const char kOverrideModelKey[] = "override_model";
const char kEnforceRankerKey[] = "enforce_ranker";
const char kOverrideModelGeoValue[] = "geo";
const char kOverrideModelDefaultValue[] = "default";
const char kContentLanguagesDisableObserversParam[] = "disable_observers";

OverrideLanguageModel GetOverrideLanguageModel() {
  std::map<std::string, std::string> params;
  bool should_override_model = base::GetFieldTrialParamsByFeature(
      kOverrideTranslateTriggerInIndia, &params);

  // Note: when there are multiple possible override models, the overrides
  // ordering is important as it allows us to have concurrent overrides in
  // experiment without having to partition them explicitly.

  if (should_override_model &&
      params[kOverrideModelKey] == kOverrideModelGeoValue) {
    return OverrideLanguageModel::GEO;
  }

  return OverrideLanguageModel::DEFAULT;
}

bool ShouldForceTriggerTranslateOnEnglishPages(int force_trigger_count) {
  if (!base::FeatureList::IsEnabled(kOverrideTranslateTriggerInIndia))
    return false;

  return !IsForceTriggerBackoffThresholdReached(force_trigger_count);
}

bool ShouldPreventRankerEnforcementInIndia(int force_trigger_count) {
  std::map<std::string, std::string> params;
  return base::FeatureList::IsEnabled(kOverrideTranslateTriggerInIndia) &&
         !IsForceTriggerBackoffThresholdReached(force_trigger_count) &&
         base::GetFieldTrialParamsByFeature(kOverrideTranslateTriggerInIndia,
                                            &params) &&
         params[kEnforceRankerKey] == "false";
}

bool IsForceTriggerBackoffThresholdReached(int force_trigger_count) {
  int threshold;
  std::map<std::string, std::string> params;
  if (!base::GetFieldTrialParamsByFeature(kOverrideTranslateTriggerInIndia,
                                          &params) ||
      !base::StringToInt(params[kBackoffThresholdKey], &threshold)) {
    return false;
  }

  return force_trigger_count >= threshold;
}

}  // namespace language
