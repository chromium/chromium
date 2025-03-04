// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_feature_configs.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
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
             base::FEATURE_ENABLED_BY_DEFAULT);

SearchAggregatorProvider::SearchAggregatorProvider() {
  enabled = base::FeatureList::IsEnabled(kSearchAggregatorProvider);

  min_query_length =
      base::FeatureParam<int>(&kSearchAggregatorProvider, "min_query_length", 4)
          .Get();
  parse_response_in_utility_process =
      base::FeatureParam<bool>(&kSearchAggregatorProvider,
                               "parse_response_in_utility_process", true)
          .Get();
  use_discovery_engine_oauth_scope =
      base::FeatureParam<bool>(&kSearchAggregatorProvider,
                               "use_discovery_engine_oauth_scope", false)
          .Get();
  disable_drive = base::FeatureParam<bool>(&kSearchAggregatorProvider,
                                           "disable_drive", true)
                      .Get();

  name = base::FeatureParam<std::string>(&kSearchAggregatorProvider, "name", "")
             .Get();
  shortcut = base::FeatureParam<std::string>(&kSearchAggregatorProvider,
                                             "shortcut", "")
                 .Get();
  search_url = base::FeatureParam<std::string>(&kSearchAggregatorProvider,
                                               "search_url", "")
                   .Get();
  suggest_url = base::FeatureParam<std::string>(&kSearchAggregatorProvider,
                                                "suggest_url", "")
                    .Get();
  icon_url = base::FeatureParam<std::string>(&kSearchAggregatorProvider,
                                             "icon_url", "")
                 .Get();
  require_shortcut = base::FeatureParam<bool>(&kSearchAggregatorProvider,
                                              "require_shortcut", false)
                         .Get();
}

SearchAggregatorProvider::SearchAggregatorProvider(
    const SearchAggregatorProvider&) = default;

SearchAggregatorProvider& SearchAggregatorProvider::operator=(
    const SearchAggregatorProvider&) = default;

SearchAggregatorProvider::~SearchAggregatorProvider() = default;

bool SearchAggregatorProvider::AreMockEnginesValid() const {
  return enabled && !shortcut.empty() && shortcut[0] != '@' && !name.empty() &&
         !search_url.empty() &&
         search_url.find("{searchTerms}") != std::string::npos &&
         !suggest_url.empty();
}

std::vector<base::Value> SearchAggregatorProvider::CreateMockSearchEngines()
    const {
  std::vector<base::Value> engines;
  engines.emplace_back(CreateMockSearchAggregator(/*featured_by_policy=*/true));
  engines.emplace_back(
      CreateMockSearchAggregator(/*featured_by_policy=*/false));
  return engines;
}

base::Value::Dict SearchAggregatorProvider::CreateMockSearchAggregator(
    bool featured_by_policy) const {
  CHECK(AreMockEnginesValid());

  base::Value::Dict result;
  result.Set("short_name", name);
  result.Set("keyword", featured_by_policy ? '@' + shortcut : shortcut);
  result.Set("url", search_url);
  result.Set("suggestions_url", suggest_url);
  if (!icon_url.empty()) {
    result.Set("favicon_url", icon_url);
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

BASE_FEATURE(UrlSuggestionsOnFocus::kUrlSuggestionsOnFocus,
             "UrlSuggestionsOnFocus",
             base::FEATURE_DISABLED_BY_DEFAULT);
UrlSuggestionsOnFocus::UrlSuggestionsOnFocus() {
  enabled = base::FeatureList::IsEnabled(kUrlSuggestionsOnFocus);
  max_suggestions = base::FeatureParam<size_t>(&kUrlSuggestionsOnFocus,
                                               "OnFocusMaxSearchSuggestions", 8)
                        .Get();
  max_search_suggestions =
      base::FeatureParam<size_t>(&kUrlSuggestionsOnFocus,
                                 "OnFocusMaxSearchSuggestions", 4)
          .Get();
  max_url_suggestions =
      base::FeatureParam<size_t>(&kUrlSuggestionsOnFocus,
                                 "OnFocusMaxUrlSuggestions", 4)
          .Get();
}
}  // namespace omnibox_feature_configs
