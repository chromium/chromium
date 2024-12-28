// Copyright 2024 The Chromium Authors
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
  scope_backoff_to_profile =
      base::FeatureParam<bool>(&omnibox::kDocumentProvider,
                               "DocumentProviderScopeBackoffToProfile", false)
          .Get();
  backoff_duration = base::FeatureParam<base::TimeDelta>(
                         &omnibox::kDocumentProvider,
                         "DocumentProviderBackoffDuration", base::TimeDelta())
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
BASE_FEATURE(SearchAggregatorProvider::kSearchAggregatorProvider,
             "SearchAggregatorProvider",
             base::FEATURE_DISABLED_BY_DEFAULT);

SearchAggregatorProvider::SearchAggregatorProvider() {
  Init(
      /*enabled=*/base::FeatureList::IsEnabled(kSearchAggregatorProvider),
      /*name=*/
      base::FeatureParam<std::string>(&kSearchAggregatorProvider, "name", "")
          .Get(),
      /*shortcut=*/
      base::FeatureParam<std::string>(&kSearchAggregatorProvider, "shortcut",
                                      "")
          .Get(),
      /*search_url=*/
      base::FeatureParam<std::string>(&kSearchAggregatorProvider, "search_url",
                                      "")
          .Get(),
      /*suggest_url=*/
      base::FeatureParam<std::string>(&kSearchAggregatorProvider, "suggest_url",
                                      "")
          .Get(),
      /*icon_url=*/
      base::FeatureParam<std::string>(&kSearchAggregatorProvider, "icon_url",
                                      "")
          .Get(),
      /*trigger_omnibox_blending=*/
      base::FeatureParam<bool>(&kSearchAggregatorProvider,
                               "trigger_omnibox_blending", false)
          .Get());
}

SearchAggregatorProvider::SearchAggregatorProvider(
    const SearchAggregatorProvider&) = default;

SearchAggregatorProvider& SearchAggregatorProvider::operator=(
    const SearchAggregatorProvider&) = default;

SearchAggregatorProvider::~SearchAggregatorProvider() = default;

std::vector<base::Value> SearchAggregatorProvider::GetSearchEngines() const {
  std::vector<base::Value> engines;
  if (valid_search_engine()) {
    engines.emplace_back(
        CreateMockSearchAggregator(/*featured_by_policy=*/true));
    engines.emplace_back(
        CreateMockSearchAggregator(/*featured_by_policy=*/false));
  }
  return engines;
}

void SearchAggregatorProvider::Init(bool enabled,
                                    const std::string& name,
                                    const std::string& shortcut,
                                    const std::string& search_url,
                                    const std::string& suggest_url,
                                    const std::string& icon_url,
                                    bool trigger_omnibox_blending) {
  enabled_ = enabled;
  if (!enabled_) {
    return;
  }

  name_ = name;
  shortcut_ = shortcut;
  search_url_ = search_url;
  suggest_url_ = suggest_url;
  icon_url_ = icon_url;
  trigger_omnibox_blending_ = trigger_omnibox_blending;

  // Perform some soft validation to prevent crashes downstream.
  valid_search_engine_ =
      !shortcut_.empty() && shortcut_[0] != '@' && !name_.empty() &&
      !search_url_.empty() &&
      search_url_.find("{searchTerms}") != std::string::npos &&
      !suggest_url_.empty();
  if (!valid_search_engine_) {
    VLOG(2) << "Search aggregator injected by field trial is invalid";
    return;
  }
}

void SearchAggregatorProvider::Init(bool enabled,
                                    bool trigger_omnibox_blending) {
  Init(/*enabled=*/enabled,
       /*name=*/"",
       /*shortcut=*/"",
       /*search_url=*/"",
       /*suggest_url=*/"",
       /*icon_url=*/"",
       /*trigger_omnibox_blending=*/trigger_omnibox_blending);
}

base::Value::Dict SearchAggregatorProvider::CreateMockSearchAggregator(
    bool featured_by_policy) const {
  base::Value::Dict result;
  result.Set("short_name", name_);
  result.Set("keyword", featured_by_policy ? '@' + shortcut_ : shortcut_);
  result.Set("url", search_url_);
  result.Set("suggestions_url", suggest_url_);
  if (!icon_url_.empty()) {
    result.Set("favicon_url", icon_url_);
  }

  result.Set("policy_origin",
             3 /*TemplateURLData::PolicyOrigin::kSearchAggregator*/);
  result.Set("enforced_by_policy", false);
  result.Set("featured_by_policy", featured_by_policy);
  result.Set("is_active", 1 /*TemplateURLData::ActiveStatus::kTrue*/);
  result.Set("safe_for_autoreplace", false);

  double timestamp = static_cast<double>(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  result.Set("date_created", timestamp);
  result.Set("last_modified", timestamp);
  return result;
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
