// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_feature_configs.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/omnibox/common/omnibox_features.h"

namespace omnibox_feature_configs {

constexpr auto enabled_by_default_desktop_only =
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    base::FEATURE_DISABLED_BY_DEFAULT;
#else
    base::FEATURE_ENABLED_BY_DEFAULT;
#endif

// TODO(manukh): Enabled by default in m120. Clean up 12/5 when after m121
//   branch cut.
// static
BASE_FEATURE(CalcProvider::kCalcProvider,
             "OmniboxCalcProvider",
             enabled_by_default_desktop_only);
CalcProvider::CalcProvider() {
  enabled = base::FeatureList::IsEnabled(kCalcProvider);
  score =
      base::FeatureParam<int>(&kCalcProvider, "CalcProviderScore", 900).Get();
  max_matches =
      base::FeatureParam<int>(&kCalcProvider, "CalcProviderMaxMatches", 5)
          .Get();
  num_non_calc_inputs =
      base::FeatureParam<int>(&kCalcProvider, "CalcProviderNumNonCalcInputs", 3)
          .Get();
}

DocumentProvider::DocumentProvider() {
  enabled = base::FeatureList::IsEnabled(omnibox::kDocumentProvider);
  min_query_length =
      base::FeatureParam<int>(&omnibox::kDocumentProvider,
                              "DocumentProviderMinQueryLength", 4)
          .Get();
  ignore_when_debouncing =
      base::FeatureParam<bool>(&omnibox::kDocumentProvider,
                               "DocumentProviderIgnoreWhenDebouncing", false)
          .Get();
  backoff_on_401 =
      base::FeatureParam<bool>(&omnibox::kDocumentProvider,
                               "DocumentProviderBackoffOn401", false)
          .Get();
}

// static
BASE_FEATURE(ForceAllowedToBeDefault::kForceAllowedToBeDefault,
             "OmniboxForceAllowedToBeDefault",
             base::FEATURE_DISABLED_BY_DEFAULT);
ForceAllowedToBeDefault::ForceAllowedToBeDefault() {
  enabled = base::FeatureList::IsEnabled(kForceAllowedToBeDefault);
}

// static
BASE_FEATURE(RealboxContextualAndTrendingSuggestions::
                 kRealboxContextualAndTrendingSuggestions,
             "NTPRealboxContextualAndTrendingSuggestions",
             base::FEATURE_ENABLED_BY_DEFAULT);
RealboxContextualAndTrendingSuggestions::
    RealboxContextualAndTrendingSuggestions() {
  enabled =
      base::FeatureList::IsEnabled(kRealboxContextualAndTrendingSuggestions);
  total_limit = base::FeatureParam<int>(
                    &kRealboxContextualAndTrendingSuggestions, "TotalLimit", 4)
                    .Get();
  contextual_suggestions_limit =
      base::FeatureParam<int>(&kRealboxContextualAndTrendingSuggestions,
                              "ContextualSuggestionsLimit", 4)
          .Get();
  trending_suggestions_limit =
      base::FeatureParam<int>(&kRealboxContextualAndTrendingSuggestions,
                              "TrendingSuggestionsLimit", 4)
          .Get();
}

// static
BASE_FEATURE(ReportNumZPSInSession::kReportNumZPSInSession,
             "ReportNumZPSInSession",
             base::FEATURE_ENABLED_BY_DEFAULT);
ReportNumZPSInSession::ReportNumZPSInSession()
    : enabled(base::FeatureList::IsEnabled(kReportNumZPSInSession)) {}

// static
BASE_FEATURE(ShortcutBoosting::kShortcutBoost,
             "OmniboxShortcutBoost",
             base::FEATURE_ENABLED_BY_DEFAULT);
ShortcutBoosting::ShortcutBoosting() {
  enabled = base::FeatureList::IsEnabled(kShortcutBoost);
  search_score =
      base::FeatureParam<int>(&kShortcutBoost, "ShortcutBoostSearchScore", 0)
          .Get();
  url_score =
      base::FeatureParam<int>(&kShortcutBoost, "ShortcutBoostUrlScore", 1414)
          .Get();
  counterfactual = base::FeatureParam<bool>(
                       &kShortcutBoost, "ShortcutBoostCounterfactual", false)
                       .Get();
  non_top_hit_threshold =
      base::FeatureParam<int>(&kShortcutBoost,
                              "ShortcutBoostNonTopHitThreshold", 2)
          .Get();
  non_top_hit_search_threshold =
      base::FeatureParam<int>(&kShortcutBoost,
                              "ShortcutBoostNonTopHitSearchThreshold", 2)
          .Get();
  group_with_searches =
      base::FeatureParam<bool>(&kShortcutBoost,
                               "ShortcutBoostGroupWithSearches", true)
          .Get();
}

// static
BASE_FEATURE(SuggestionAnswerMigration::kOmniboxSuggestionAnswerMigration,
             "OmniboxSuggestionAnswerMigration",
             base::FEATURE_ENABLED_BY_DEFAULT);
SuggestionAnswerMigration::SuggestionAnswerMigration() {
  enabled = base::FeatureList::IsEnabled(kOmniboxSuggestionAnswerMigration);
}

// static
BASE_FEATURE(VitalizeAutocompletedKeywords::kVitalizeAutocompletedKeywords,
             "OmniboxVitalizeAutocompletedKeywords",
             base::FEATURE_DISABLED_BY_DEFAULT);
VitalizeAutocompletedKeywords::VitalizeAutocompletedKeywords() {
  enabled = base::FeatureList::IsEnabled(kVitalizeAutocompletedKeywords);
  score = base::FeatureParam<int>(&kVitalizeAutocompletedKeywords,
                                  "VitalizeAutocompletedKeywordsScore", 450)
              .Get();
}

}  // namespace omnibox_feature_configs
