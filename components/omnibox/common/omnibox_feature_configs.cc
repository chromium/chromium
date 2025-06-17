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

BASE_FEATURE(AutocompleteControllerMetricsOptimization::
                 kAutocompleteControllerMetricsOptimization,
             "AutocompleteControllerMetricsOptimization",
             base::FEATURE_ENABLED_BY_DEFAULT);
AutocompleteControllerMetricsOptimization::
    AutocompleteControllerMetricsOptimization() {
  enabled =
      base::FeatureList::IsEnabled(kAutocompleteControllerMetricsOptimization);
}

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

BASE_FEATURE(ContextualSearch::kContextualSuggestionsAblateOthersWhenPresent,
             "ContextualSuggestionsAblateOthersWhenPresent",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Meta-feature that enables/disables the other related features if set.
// When not overridden, each feature is enabled/disabled separately.
BASE_FEATURE(ContextualSearch::kOmniboxContextualSuggestions,
             "OmniboxContextualSuggestions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the @page starter pack scope.
BASE_FEATURE(ContextualSearch::kStarterPackPage,
             "StarterPackPage",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Contextual zero-prefix (aka zero-suggest). There are suggestions based on the
// user's current URL. Fullfillment of these suggestions is delegated to Lens
// since Lens provides additional logic for contextualizing the results to the
// current page, by using more than the URL, i.e. the page content.
BASE_FEATURE(ContextualSearch::kContextualZeroSuggestLensFulfillment,
             "ContextualZeroSuggestLensFulfillment",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the contextual search provider to wait for the Lens suggest inputs
// to be ready before making the suggest request.
BASE_FEATURE(ContextualSearch::kContextualSearchProviderAsyncSuggestInputs,
             "ContextualSearchProviderAsyncSuggestInputs",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature to enable use of the "ctxus" param on zero suggest requests.
BASE_FEATURE(ContextualSearch::kSendContextualUrlSuggestParam,
             "SendContextualUrlSuggestParam",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(ContextualSearch::kOmniboxContextualSearchOnFocusSuggestions,
             "OmniboxContextualSearchOnFocusSuggestions",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(ContextualSearch::kContextualSearchBoxUsesContextualSearchProvider,
             "ContextualSearchBoxUsesContextualSearchProvider",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(ContextualSearch::kOmniboxZeroSuggestSynchronousMatchesOnly,
             "OmniboxZeroSuggestSynchronousMatchesOnly",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(ContextualSearch::kContextualSearchOpenLensActionUsesThumbnail,
             "ContextualSearchOpenLensActionUsesThumbnail",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(ContextualSearch::kSendPageTitleSuggestParam,
             "SendPageTitleSuggestParam",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(ContextualSearch::kContextualSearchAlternativeActionLabel,
             "ContextualSearchAlternativeActionLabel",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(ContextualSearch::kUseApcPaywallSignal,
             "UseApcPaywallSignal",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(ContextualSearch::kShowSuggestionsOnNoApc,
             "ShowSuggestionsOnNoApc",
             base::FEATURE_DISABLED_BY_DEFAULT);

ContextualSearch::ContextualSearch() {
  // Meta-feature turns on/off other features, but only if it's overridden by
  // the user. If not then each feature is controlled separately.
  const std::optional<bool> meta_state =
      base::FeatureList::GetStateIfOverridden(kOmniboxContextualSuggestions);
  const auto feature_enabled = [&](const base::Feature& feature) {
    return meta_state.value_or(base::FeatureList::IsEnabled(feature));
  };

  contextual_suggestions_ablate_others_when_present =
      base::FeatureList::IsEnabled(
          kContextualSuggestionsAblateOthersWhenPresent);
  contextual_suggestions_ablate_search_only =
      base::FeatureParam<bool>(&kContextualSuggestionsAblateOthersWhenPresent,
                               "AblateSearchOnly", false)
          .Get();
  contextual_suggestions_ablate_url_only =
      base::FeatureParam<bool>(&kContextualSuggestionsAblateOthersWhenPresent,
                               "AblateUrlOnly", false)
          .Get();
  starter_pack_page = feature_enabled(kStarterPackPage);
  contextual_zero_suggest_lens_fulfillment =
      feature_enabled(kContextualZeroSuggestLensFulfillment);
  csp_async_suggest_inputs =
      feature_enabled(kContextualSearchProviderAsyncSuggestInputs);
  // This could be taken from a feature param if needed, but currently it's
  // simply one or none.
  contextual_url_suggest_param =
      feature_enabled(kSendContextualUrlSuggestParam) ? "1" : "";
  contextual_zps_limit =
      feature_enabled(kOmniboxContextualSearchOnFocusSuggestions)
          ? base::FeatureParam<int>(&kOmniboxContextualSearchOnFocusSuggestions,
                                    "Limit", 3)
                .Get()
          : 0;
  csb_uses_csp = base::FeatureList::IsEnabled(
      kContextualSearchBoxUsesContextualSearchProvider);
  zero_suggest_synchronous_matches_only =
      base::FeatureList::IsEnabled(kOmniboxZeroSuggestSynchronousMatchesOnly);
  open_lens_action_uses_thumbnail = base::FeatureList::IsEnabled(
      kContextualSearchOpenLensActionUsesThumbnail);
  send_page_title_suggest_param = feature_enabled(kSendPageTitleSuggestParam);
  alternative_action_label =
      base::FeatureParam<int>(&kContextualSearchAlternativeActionLabel,
                              "LabelIndex", 0)
          .Get();
  show_open_lens_action =
      feature_enabled(kOmniboxContextualSearchOnFocusSuggestions);
  use_apc_paywall_signal = feature_enabled(kUseApcPaywallSignal);
  show_suggestions_on_no_apc =
      base::FeatureList::IsEnabled(kShowSuggestionsOnNoApc);
}

ContextualSearch::ContextualSearch(const ContextualSearch&) = default;
ContextualSearch& ContextualSearch::operator=(const ContextualSearch&) =
    default;
ContextualSearch::~ContextualSearch() = default;

bool ContextualSearch::IsContextualSearchEnabled() const {
  return show_open_lens_action;
}

bool ContextualSearch::IsEnabledWithPrefetch() const {
  return IsContextualSearchEnabled() && zero_suggest_synchronous_matches_only;
}

BASE_FEATURE(MiaZPS::kOmniboxMiaZPS,
             "OmniboxMiaZPS",
             base::FEATURE_DISABLED_BY_DEFAULT);

MiaZPS::MiaZPS() {
  enabled = base::FeatureList::IsEnabled(kOmniboxMiaZPS);
  local_history_non_normalized_contents =
      base::FeatureParam<bool>(&kOmniboxMiaZPS,
                               "LocalHistoryNonNormalizedContents", true)
          .Get();

  suppress_psuggest_backfill_with_mia =
      base::FeatureParam<bool>(&kOmniboxMiaZPS,
                               "SuppressPsuggestBackfillWithMIA", false)
          .Get();
}

DocumentProvider::DocumentProvider() {
  enabled = base::FeatureList::IsEnabled(omnibox::kDocumentProvider);
  min_query_length =
      base::FeatureParam<int>(&omnibox::kDocumentProvider,
                              "DocumentProviderMinQueryLength", 4)
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

BASE_FEATURE(AdjustOmniboxIndent::kAdjustOmniboxIndent,
             "AdjustOmniboxIndent",
             base::FEATURE_DISABLED_BY_DEFAULT);

AdjustOmniboxIndent::AdjustOmniboxIndent() {
  const bool enabled = base::FeatureList::IsEnabled(kAdjustOmniboxIndent);
  indent_input_when_popup_closed =
      enabled ? base::FeatureParam<bool>(&kAdjustOmniboxIndent,
                                         "indent-input-when-popup-closed", true)
                    .Get()
              : false;
  input_icon_indent_offset =
      enabled ? base::FeatureParam<int>(&kAdjustOmniboxIndent,
                                        "input-icon-indent-offset", -7)
                    .Get()
              : 0;
  input_text_indent_offset =
      enabled ? base::FeatureParam<int>(&kAdjustOmniboxIndent,
                                        "input-text-indent-offset", -2)
                    .Get()
              : 0;
  match_icon_indent_offset =
      enabled ? base::FeatureParam<int>(&kAdjustOmniboxIndent,
                                        "match-icon-indent-offset", -7)
                    .Get()
              : 0;
  match_text_indent_offset =
      enabled ? base::FeatureParam<int>(&kAdjustOmniboxIndent,
                                        "match-text-indent-offset", -9)
                    .Get()
              : 0;
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
                               "use_discovery_engine_oauth_scope", true)
          .Get();
  disable_drive = base::FeatureParam<bool>(&kSearchAggregatorProvider,
                                           "disable_drive", true)
                      .Get();
  multiple_requests = base::FeatureParam<bool>(&kSearchAggregatorProvider,
                                               "multiple_requests", true)
                          .Get();

  relevance_scoring_mode =
      base::FeatureParam<std::string>(&kSearchAggregatorProvider,
                                      "relevance_scoring_mode", "mixed")
          .Get();

  realbox_unscoped_suggestions =
      base::FeatureParam<bool>(&kSearchAggregatorProvider,
                               "realbox_unscoped_suggestions", true)
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
  result.Set("enforced_by_policy", true);
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

BASE_FEATURE(OmniboxZpsSuggestionLimit::kOmniboxZpsSuggestionLimit,
             "OmniboxZpsSuggestionLimit",
             base::FEATURE_DISABLED_BY_DEFAULT);
OmniboxZpsSuggestionLimit::OmniboxZpsSuggestionLimit() {
  enabled = base::FeatureList::IsEnabled(kOmniboxZpsSuggestionLimit);
  max_suggestions = base::FeatureParam<size_t>(&kOmniboxZpsSuggestionLimit,
                                               "OmniboxZpsMaxSuggestions", 6)
                        .Get();
  max_search_suggestions =
      base::FeatureParam<size_t>(&kOmniboxZpsSuggestionLimit,
                                 "OmniboxZpsMaxSearchSuggestions", 3)
          .Get();
  max_url_suggestions =
      base::FeatureParam<size_t>(&kOmniboxZpsSuggestionLimit,
                                 "OmniboxZpsMaxUrlSuggestions", 3)
          .Get();
}

BASE_FEATURE(OmniboxUrlSuggestionsOnFocus::kOmniboxUrlSuggestionsOnFocus,
             "OmniboxUrlSuggestionsOnFocus",
             base::FEATURE_DISABLED_BY_DEFAULT);
OmniboxUrlSuggestionsOnFocus::OmniboxUrlSuggestionsOnFocus() {
  const char kMvtScoringParamRecencyFactor_Classic[] = "default";
  enabled = base::FeatureList::IsEnabled(kOmniboxUrlSuggestionsOnFocus);
  show_recently_closed_tabs =
      base::FeatureParam<bool>(&kOmniboxUrlSuggestionsOnFocus,
                               "ShowRecentlyClosedTabs", false)
          .Get();
  most_visited_recency_window =
      base::FeatureParam<size_t>(&kOmniboxUrlSuggestionsOnFocus,
                                 "OnFocusMostVisitedRecencyWindow", 0)
          .Get();
  most_visited_recency_factor =
      base::FeatureParam<std::string>(&kOmniboxUrlSuggestionsOnFocus,
                                      "OnFocusMostVisitedRecencyFactor",
                                      kMvtScoringParamRecencyFactor_Classic)
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
  max_requested_urls_from_history =
      base::FeatureParam<size_t>(&kOmniboxUrlSuggestionsOnFocus,
                                 "MaxRequestedUrlsFromHistory", 500)
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
  happiness_trigger_id = base::FeatureParam<std::string>(
                             &kHappinessTrackingSurveyForOmniboxOnFocusZps,
                             "HappinessTriggerId", "")
                             .Get();
  utility_trigger_id =
      base::FeatureParam<std::string>(
          &kHappinessTrackingSurveyForOmniboxOnFocusZps, "UtilityTriggerId", "")
          .Get();
}
}  // namespace omnibox_feature_configs
