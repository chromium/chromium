// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_CORE_FEATURES_H_
#define COMPONENTS_PAGE_INFO_CORE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace page_info {

// Enables the "About this site" section in Page Info.
extern bool IsAboutThisSiteFeatureEnabled(const std::string& locale);

// Controls the feature for English and other languages that are enabled by
// default. Use IsAboutThisSiteFeatureEnabled() to check a specific language.
BASE_DECLARE_FEATURE(kPageInfoAboutThisSite);
// Controls the feature for languages that are not enabled by default yet.
BASE_DECLARE_FEATURE(kPageInfoAboutThisSiteMoreLangs);

// Whether we show hard-coded content for some sites like https://example.com.
extern const base::FeatureParam<bool> kShowSampleContent;

// Enables the merchant trust section for Page Info.
BASE_DECLARE_FEATURE(kMerchantTrust);

extern const char kMerchantTrustEnabledWithSampleDataName[];
extern const base::FeatureParam<bool> kMerchantTrustEnabledWithSampleData;

extern const char kMerchantTrustForceShowUIForTestingName[];
extern const base::FeatureParam<bool> kMerchantTrustForceShowUIForTesting;

extern const char kMerchantTrustEnableOmniboxChipName[];
extern const base::FeatureParam<bool> kMerchantTrustEnableOmniboxChip;

// Enables the merchant trust UI even when the shopper voice summary is missing.
extern const char kMerchantTrustWithoutSummaryName[];
extern const base::FeatureParam<bool> kMerchantTrustWithoutSummary;

// Whether the merchant trust UI should be shown even when the shopper voice
// summary is missing.
extern bool IsMerchantTrustWithoutSummaryEnabled();

// Whether the merchant trust section for Page Info based on country and locale.
extern bool IsMerchantTrustFeatureEnabled(const std::string& country_code,
                                          const std::string& locale);

// Enables the 'Merchant trust' sentiment survey for control group. Used for
// feature evaluation. This should be set in the fieldtrial config along with
// the trigger ID for the corresponding survey (as en_site_id) and probability
// (as probability).
BASE_DECLARE_FEATURE(kMerchantTrustEvaluationControlSurvey);

// A minimum amount of time, that has to pass after visiting a shopping page,
// before trying to show a survey.
extern const base::FeatureParam<base::TimeDelta>
    kMerchantTrustEvaluationControlMinTimeToShowSurvey;

// A maximum amount of time, that has passed after visiting a shopping page,
// during which we can show a survey.
extern const base::FeatureParam<base::TimeDelta>
    kMerchantTrustEvaluationControlMaxTimeToShowSurvey;

// Enables the 'Merchant trust' sentiment survey for experiment group. Used for
// feature evaluation. The survey will be attempted to be shown on a new tab
// when all the conditions apply. This should be set in the fieldtrial config
// along with the trigger ID for the corresponding survey (as en_site_id) and
// probability (as probability).
BASE_DECLARE_FEATURE(kMerchantTrustEvaluationExperimentSurvey);

// A minimum amount of time, that has to pass after seeing 'Merchant trust'
// feature, before trying to show a survey.
extern const base::FeatureParam<base::TimeDelta>
    kMerchantTrustEvaluationExperimentMinTimeToShowSurvey;

// A maximum amount of time, that has passed after seeing 'Merchant trust'
// feature, during which we can show a survey.
extern const base::FeatureParam<base::TimeDelta>
    kMerchantTrustEvaluationExperimentMaxTimeToShowSurvey;

// A minimal duration of the interaction with Merchant Trust UI required to show
// the survey.
extern const base::FeatureParam<base::TimeDelta>
    kMerchantTrustRequiredInteractionDuration;

// Enables the 'Merchant trust' sentiment survey that will be available for
// users in the page info. Feature params are set directly in the code as due to
// finch / fieldtrial config limitations we can't configure multiple surveys
// with same params name but different values in distinct features within the
// same config group.
BASE_DECLARE_FEATURE(kMerchantTrustLearnSurvey);

// The trigger ID of the learn survey to be shown.
extern const base::FeatureParam<std::string> kMerchantTrustLearnSurveyTriggerId;

// The probability of showing the learn survey.
extern const base::FeatureParam<double> kMerchantTrustLearnSurveyProbability;

// Whether this survey is user prompted.
extern const base::FeatureParam<bool> kMerchantTrustLearnSurveyUserPrompted;

}  // namespace page_info

#endif  // COMPONENTS_PAGE_INFO_CORE_FEATURES_H_
