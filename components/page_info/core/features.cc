// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/features.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"

namespace page_info {
constexpr auto kDefaultLangs = base::MakeFixedFlatSet<std::string_view>({
    "ar", "bg", "ca", "cs", "da", "de", "el", "en", "es", "et",
    "fi", "fr", "he", "hi", "hr", "hu", "id", "it", "ja", "ko",
    "lt", "lv", "nb", "nl", "pl", "pt", "ro", "ru", "sk", "sl",
    "sr", "sv", "sw", "th", "tr", "uk", "vi", "zh",
});

extern bool IsAboutThisSiteFeatureEnabled(const std::string& locale) {
  if (base::Contains(kDefaultLangs, l10n_util::GetLanguage(locale))) {
    return base::FeatureList::IsEnabled(kPageInfoAboutThisSite);
  }
  return base::FeatureList::IsEnabled(kPageInfoAboutThisSiteMoreLangs);
}

BASE_FEATURE(kPageInfoAboutThisSite, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kPageInfoAboutThisSiteMoreLangs,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kShowSampleContent{&kPageInfoAboutThisSite,
                                                  "ShowSampleContent", false};

BASE_FEATURE(kMerchantTrust, base::FEATURE_ENABLED_BY_DEFAULT);

const char kMerchantTrustEnabledWithSampleDataName[] =
    "enabled-with-sample-data";
const base::FeatureParam<bool> kMerchantTrustEnabledWithSampleData{
    &kMerchantTrust, kMerchantTrustEnabledWithSampleDataName, false};

const char kMerchantTrustEnabledForCountry[] = "us";
const char kMerchantTrustEnabledForLocale[] = "en-us";

const char kMerchantTrustForceShowUIForTestingName[] =
    "force-show-ui-for-testing";
const base::FeatureParam<bool> kMerchantTrustForceShowUIForTesting{
    &kMerchantTrust, kMerchantTrustForceShowUIForTestingName, false};

const char kMerchantTrustEnableOmniboxChipName[] = "enable-omnibox-chip";
const base::FeatureParam<bool> kMerchantTrustEnableOmniboxChip{
    &kMerchantTrust, kMerchantTrustEnableOmniboxChipName, false};

const char kMerchantTrustWithoutSummaryName[] = "enable-without-summary";
const base::FeatureParam<bool> kMerchantTrustWithoutSummary{
    &kMerchantTrust, kMerchantTrustWithoutSummaryName, true};

bool IsMerchantTrustWithoutSummaryEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kMerchantTrust, kMerchantTrustWithoutSummaryName, true);
}

extern bool IsMerchantTrustFeatureEnabled(const std::string& country_code,
                                          const std::string& locale) {
  if (kMerchantTrustForceShowUIForTesting.Get()) {
    return true;
  }

  return base::FeatureList::IsEnabled(kMerchantTrust) &&
         base::ToLowerASCII(country_code) == kMerchantTrustEnabledForCountry &&
         base::ToLowerASCII(locale) == kMerchantTrustEnabledForLocale;
}

BASE_FEATURE(kMerchantTrustEvaluationControlSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta>
    kMerchantTrustEvaluationControlMinTimeToShowSurvey{
        &kMerchantTrustEvaluationControlSurvey, "MinTimeToShowSurvey",
        base::Minutes(2)};

const base::FeatureParam<base::TimeDelta>
    kMerchantTrustEvaluationControlMaxTimeToShowSurvey{
        &kMerchantTrustEvaluationControlSurvey, "MaxTimeToShowSurvey",
        base::Minutes(60)};

BASE_FEATURE(kMerchantTrustEvaluationExperimentSurvey,
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta>
    kMerchantTrustEvaluationExperimentMinTimeToShowSurvey{
        &kMerchantTrustEvaluationExperimentSurvey, "MinTimeToShowSurvey",
        base::Minutes(2)};

const base::FeatureParam<base::TimeDelta>
    kMerchantTrustEvaluationExperimentMaxTimeToShowSurvey{
        &kMerchantTrustEvaluationExperimentSurvey, "MaxTimeToShowSurvey",
        base::Minutes(60)};

const base::FeatureParam<base::TimeDelta>
    kMerchantTrustRequiredInteractionDuration{
        &kMerchantTrustEvaluationExperimentSurvey,
        "RequiredInteractionDuration", base::Seconds(5)};

BASE_FEATURE(kMerchantTrustLearnSurvey, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kMerchantTrustLearnSurveyTriggerId{
    &kMerchantTrustLearnSurvey, "trigger_id", "EA14LFXPG0ugnJ3q1cK0Y6Gtj3De"};

extern const base::FeatureParam<double> kMerchantTrustLearnSurveyProbability{
    &kMerchantTrustLearnSurvey, "probability", 1.0};

extern const base::FeatureParam<bool> kMerchantTrustLearnSurveyUserPrompted{
    &kMerchantTrustLearnSurvey, "user_prompted", true};

}  // namespace page_info
