// Copyright 2018 The Chromium Authors. All rights reserved.
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
const base::Feature kUseHeuristicLanguageModel{
    "UseHeuristicLanguageModel", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kOverrideTranslateTriggerInIndia{
    "OverrideTranslateTriggerInIndia", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kExplicitLanguageAsk{"ExplicitLanguageAsk",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kUseFluentLanguageModel {
  "UseFluentLanguageModel",
#if defined(OS_IOS)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};
const base::Feature kNotifySyncOnLanguageDetermined{
    "NotifySyncOnLanguageDetermined", base::FEATURE_ENABLED_BY_DEFAULT};

// Base feature for Translate desktop UI experiment
const base::Feature kUseButtonTranslateBubbleUi{
    "UseButtonTranslateBubbleUI", base::FEATURE_DISABLED_BY_DEFAULT};

// Params:
const char kBackoffThresholdKey[] = "backoff_threshold";
const char kOverrideModelKey[] = "override_model";
const char kEnforceRankerKey[] = "enforce_ranker";
const char kOverrideModelHeuristicValue[] = "heuristic";
const char kOverrideModelGeoValue[] = "geo";
const char kOverrideModelDefaultValue[] = "default";

// Params for Translate Desktop UI experiment
const char kTranslateUIBubbleKey[] = "translate_ui_bubble_style";
const char kTranslateUIBubbleButtonValue[] = "button";
const char kTranslateUIBubbleTabValue[] = "tab";
const char kTranslateUIBubbleButtonGM2Value[] = "button_gm2";

OverrideLanguageModel GetOverrideLanguageModel() {
  std::map<std::string, std::string> params;
  bool should_override_model = base::GetFieldTrialParamsByFeature(
      kOverrideTranslateTriggerInIndia, &params);

  // The model overrides ordering is important as it allows us to
  // have concurrent overrides in experiment without having to partition them
  // explicitly. For example, we may have a FLUENT experiment globally and a
  // GEO experiment in India only.

  if (base::FeatureList::IsEnabled(kUseHeuristicLanguageModel) ||
      (should_override_model &&
       params[kOverrideModelKey] == kOverrideModelHeuristicValue)) {
    return OverrideLanguageModel::HEURISTIC;
  }

  if (should_override_model &&
      params[kOverrideModelKey] == kOverrideModelGeoValue) {
    return OverrideLanguageModel::GEO;
  }

  if (base::FeatureList::IsEnabled(kUseFluentLanguageModel)) {
    return OverrideLanguageModel::FLUENT;
  }

  return OverrideLanguageModel::DEFAULT;
}

bool ShouldForceTriggerTranslateOnEnglishPages(int force_trigger_count) {
  if (!base::FeatureList::IsEnabled(kOverrideTranslateTriggerInIndia))
    return false;

  bool threshold_reached =
      IsForceTriggerBackoffThresholdReached(force_trigger_count);
  UMA_HISTOGRAM_BOOLEAN("Translate.ForceTriggerBackoffStateReached",
                        threshold_reached);

  return !threshold_reached;
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

TranslateUIBubbleModel GetTranslateUiBubbleModel() {
  std::map<std::string, std::string> params;
  if (base::GetFieldTrialParamsByFeature(language::kUseButtonTranslateBubbleUi,
                                         &params)) {
    if (params[language::kTranslateUIBubbleKey] ==
        language::kTranslateUIBubbleButtonValue) {
      return language::TranslateUIBubbleModel::BUTTON;
    } else if (params[language::kTranslateUIBubbleKey] ==
               language::kTranslateUIBubbleTabValue) {
      return language::TranslateUIBubbleModel::TAB;
    } else if (params[language::kTranslateUIBubbleKey] ==
               language::kTranslateUIBubbleButtonGM2Value) {
      return language::TranslateUIBubbleModel::BUTTON_GM2;
    } else {
      return language::TranslateUIBubbleModel::DEFAULT;
    }
  } else {
    return language::TranslateUIBubbleModel::DEFAULT;
  }
}

}  // namespace language
