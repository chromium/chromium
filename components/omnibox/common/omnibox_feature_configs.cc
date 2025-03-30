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

  scoring_max_matches_created_per_type =
      base::FeatureParam<size_t>(&kSearchAggregatorProvider,
                                 "scoring_max_matches_created_per_type", 40)
          .Get();
  scoring_max_scoped_matches_shown_per_type =
      base::FeatureParam<size_t>(&kSearchAggregatorProvider,
                                 "scoring_max_scoped_matches_shown_per_type", 4)
          .Get();
  scoring_max_unscoped_matches_shown_per_type =
      base::FeatureParam<size_t>(&kSearchAggregatorProvider,
                                 "scoring_max_unscoped_matches_shown_per_type",
                                 2)
          .Get();
  scoring_min_char_for_strong_text_match =
      base::FeatureParam<size_t>(&kSearchAggregatorProvider,
                                 "scoring_min_char_for_strong_text_match", 3)
          .Get();
  scoring_min_words_for_full_text_match_boost =
      base::FeatureParam<size_t>(&kSearchAggregatorProvider,
                                 "scoring_min_words_for_full_text_match_boost",
                                 2)
          .Get();
  scoring_full_text_match_score =
      base::FeatureParam<int>(&kSearchAggregatorProvider,
                              "scoring_full_text_match_score", 1000)
          .Get();
  scoring_score_per_strong_text_match =
      base::FeatureParam<int>(&kSearchAggregatorProvider,
                              "scoring_score_per_strong_text_match", 400)
          .Get();
  scoring_score_per_weak_text_match =
      base::FeatureParam<int>(&kSearchAggregatorProvider,
                              "scoring_score_per_weak_text_match", 100)
          .Get();
  scoring_max_text_score =
      base::FeatureParam<int>(&kSearchAggregatorProvider,
                              "scoring_max_text_score", 800)
          .Get();
  scoring_people_score_boost =
      base::FeatureParam<int>(&kSearchAggregatorProvider,
                              "scoring_people_score_boost", 100)
          .Get();
  scoring_prefer_contents_over_queries =
      base::FeatureParam<bool>(&kSearchAggregatorProvider,
                               "scoring_prefer_contents_over_queries", true)
          .Get();
  scoring_scoped_max_low_quality_matches =
      base::FeatureParam<size_t>(&kSearchAggregatorProvider,
                                 "scoring_scoped_max_low_quality_matches", 8)
          .Get();
  scoring_unscoped_max_low_quality_matches =
      base::FeatureParam<size_t>(&kSearchAggregatorProvider,
                                 "scoring_unscoped_max_low_quality_matches", 2)
          .Get();
  // 400 + 100
  scoring_low_quality_threshold =
      base::FeatureParam<int>(&kSearchAggregatorProvider,
                              "scoring_low_quality_threshold", 500)
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

BASE_FEATURE(OmniboxUrlSuggestionsOnFocus::kOmniboxUrlSuggestionsOnFocus,
             "OmniboxUrlSuggestionsOnFocus",
             base::FEATURE_DISABLED_BY_DEFAULT);
OmniboxUrlSuggestionsOnFocus::OmniboxUrlSuggestionsOnFocus() {
  const char kMvtScoringParamRecencyFactor_Default[] = "default";
  enabled = base::FeatureList::IsEnabled(kOmniboxUrlSuggestionsOnFocus);
  show_recently_closed_tabs =
      base::FeatureParam<bool>(&kOmniboxUrlSuggestionsOnFocus,
                               "ShowRecentlyClosedTabs", false)
          .Get();
  max_suggestions = base::FeatureParam<size_t>(&kOmniboxUrlSuggestionsOnFocus,
                                               "OnFocusMaxSuggestions", 6)
                        .Get();
  max_search_suggestions =
      base::FeatureParam<size_t>(&kOmniboxUrlSuggestionsOnFocus,
                                 "OnFocusMaxSearchSuggestions", 3)
          .Get();
  max_url_suggestions =
      base::FeatureParam<size_t>(&kOmniboxUrlSuggestionsOnFocus,
                                 "OnFocusMaxUrlSuggestions", 3)
          .Get();
  most_visited_recency_window =
      base::FeatureParam<size_t>(&kOmniboxUrlSuggestionsOnFocus,
                                 "OnFocusMostVisitedRecencyWindow", 0)
          .Get();
  most_visited_recency_factor =
      base::FeatureParam<std::string>(&kOmniboxUrlSuggestionsOnFocus,
                                      "OnFocusMostVisitedRecencyFactor",
                                      kMvtScoringParamRecencyFactor_Default)
          .Get();
  directly_query_history_service =
      base::FeatureParam<bool>(&kOmniboxUrlSuggestionsOnFocus,
                               "OnFocusMostVisitedDirectlyQueryHistoryService",
                               true)
          .Get();
  prefetch_most_visited_sites =
      base::FeatureParam<bool>(&kOmniboxUrlSuggestionsOnFocus,
                               "OnFocusPrefetchMostVisitedSites", true)
          .Get();
  prefetch_most_visited_sites_delay_ms =
      base::FeatureParam<int>(&kOmniboxUrlSuggestionsOnFocus,
                              "OnFocusPrefetchDelay", 300)
          .Get();
}

OmniboxUrlSuggestionsOnFocus::OmniboxUrlSuggestionsOnFocus(
    const OmniboxUrlSuggestionsOnFocus&) = default;

OmniboxUrlSuggestionsOnFocus& OmniboxUrlSuggestionsOnFocus::operator=(
    const OmniboxUrlSuggestionsOnFocus&) = default;

OmniboxUrlSuggestionsOnFocus::~OmniboxUrlSuggestionsOnFocus() = default;

bool OmniboxUrlSuggestionsOnFocus::MostVisitedPrefetchingEnabled() const {
  return enabled && prefetch_most_visited_sites;
}

BASE_FEATURE(HappinessTrackingSurveyForOmniboxOnFocusZps::
                 kHappinessTrackingSurveyForOmniboxOnFocusZps,
             "HappinessTrackingSurveyForOmniboxOnFocusZps",
             base::FEATURE_DISABLED_BY_DEFAULT);
HappinessTrackingSurveyForOmniboxOnFocusZps::
    HappinessTrackingSurveyForOmniboxOnFocusZps() {
  enabled = base::FeatureList::IsEnabled(
      kHappinessTrackingSurveyForOmniboxOnFocusZps);
  focus_threshold =
      base::FeatureParam<size_t>(&kHappinessTrackingSurveyForOmniboxOnFocusZps,
                                 "FocusThreshold", 5)
          .Get();
  survey_delay =
      base::FeatureParam<size_t>(&kHappinessTrackingSurveyForOmniboxOnFocusZps,
                                 "SurveyDelay", 7000)
          .Get();
}
}  // namespace omnibox_feature_configs
