// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURE_CONFIGS_H_
#define COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURE_CONFIGS_H_

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "base/values.h"

class EnterpriseSearchManager;
class EnterpriseSearchManagerProviderInjectionTest;
class EnterpriseSearchManagerRequireShortcutTest;
FORWARD_DECLARE_TEST(EnterpriseSearchManagerProviderInjectionTest, Verify);

namespace omnibox_feature_configs {

/*
Finch params aren't cached. Reading the params 100's of times per omnibox
input significantly impacts metrics. Configs cache the params to avoid
regressions. 3 steps:

(1) Declare/define the config:

  // omnibox_feature_configs.h

  struct MyFeature : Config<MyFeature> {
    DECLARE_FEATURE(kMyFeature);

    MyFeature();

    bool enabled;
    int my_param;
  }

  // omnibox_feature_configs.cc

  // static
  BASE_FEATURE(MyFeature::kMyFeature, "OmniboxMyFeature",
               base::FEATURE_DISABLED_BY_DEFAULT);

  MyFeature::MyFeature() {
    enabled = base::FeatureList::IsEnabled(kMyFeature);
    my_param = base::FeatureParam<int>(&kMyFeature, "my_param", 0).Get();
  }


(2) Use the config:

  int x = omnibox_feature_configs::MyFeature::Get().my_param;


(3) Override the config in tests:

  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::MyFeature> scoped_config;
  scoped_config.Get().enabled = true;
  scoped_config.Get().my_param = 1;
  scoped_config.Reset();
  scoped_config.Get().enabled = true;
  scoped_config.Get().my_param = 2;

  instead of:

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      omnibox_feature_configs::MyFeature::kMyFeature, {{"my_param", "1"}});
  scoped_feature_list.Reset();
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      omnibox_feature_configs::MyFeature::kMyFeature, {{"my_param", "2"}});

*/

// A substitute for `BASE_DECLARE_FEATURE` for nesting in structs.
#define DECLARE_FEATURE(feature) static constinit const base::Feature feature

// Base class other configs should inherit from.
template <class T>
class Config {
 public:
  static const T& Get() {
    static T* config;
    if (config == nullptr)
      config = new T();
    return *config;
  }
};

// Util for overriding configs in tests.
template <class T>
class ScopedConfigForTesting : Config<T> {
 public:
  ScopedConfigForTesting() : original_config_(Get()) { Reset(); }
  ~ScopedConfigForTesting() { Get() = original_config_; }

  T& Get() { return const_cast<T&>(T::Get()); }
  void Reset() { Get() = {}; }

 private:
  const T original_config_;
};

// Add new configs below, ordered alphabetically.

// If enabled, adds recent calc suggestions.
struct CalcProvider : Config<CalcProvider> {
  DECLARE_FEATURE(kCalcProvider);
  CalcProvider();
  bool enabled;
  // The base score of calc suggestions.
  int score;
  // Number of calc suggestions to show.
  size_t max_matches;
  // Number of inputs that aren't a clear calculator-y input to continue showing
  // calc suggestions for.
  size_t num_non_calc_inputs;
};

// AIM related omnibox features.
struct AiMode : Config<AiMode> {
  DECLARE_FEATURE(kAllowAiModeMatches);
  DECLARE_FEATURE(kAiModeEligibility);

  AiMode();

  // Chromium-side guard for AI matches from the search server. Enabling this
  // won't guarantee AI mode matches are shown; that mostly depends on server
  // side. But disabling this will hide the server echo matches.
  bool allow_ai_mode_matches;

  // Deduping doesn't consider extra query params like `udm=50`.
  // `google.com/?q=query&udm=50` and `google.com/?q=query` would usually be
  // deduped. This param makes `udm=50` in the match's suggest template a
  // differentiating signal in deduping. Does not apply to `udm=50` in normal
  // URLs. Does not apply to e.g. `udm=49` in the suggest template.
  bool do_not_dedupe_aim_suggestions = true;

  // Navigations that match a site search update the `keyword_search_terms`
  // table in the history DB. E.g. youtube.com/id/x, google.com/?q=x, or
  // google.com/?q=x&udm=50. Determining which site search was used and should
  // be attributed does not consider query params. Navigating to either
  // google.com/?q=x or google.com/?q=x&udm=50 will each attribute both the
  // Google and AI mode site searches. When
  // `do_not_show_historic_aim_suggestions` is enabled, 2 things change:
  // 1. AI mode navigations don't increment any site search. This ensures the
  //    user won't see a traditional history search suggestion for an AI mode
  //    search they've done.
  // 2. No navigation increments the AI mode site search. This ensures the user
  //    won't see AI mode history search suggestions for a traditional search
  //    they've done.
  // These changes apply to both omnibox and other (e.g. bookmark, web)
  // navigations.
  bool do_not_show_historic_aim_suggestions = true;

  // Whether to check for AI mode eligibility on the client side
  // `AimEligibilityService` based on the user's locale.
  bool check_ai_locale_client_side = true;

  // If true, use the gws side eligibility values. Otherwise, ignore the gws
  // side response and use client side eligibility values.
  bool check_ai_eligibility_gws_side = false;
};

// If enabled, show the AIM entrypoint in the omnibox.
struct AiModeOmniboxEntryPoint : Config<AiModeOmniboxEntryPoint> {
  AiModeOmniboxEntryPoint();
  // Whether the AIM entrypoint is enabled.
  bool enabled;

  // Never display AIM hint text.
  bool hide_aim_hint_text;

  // Whether to hide the AIM hint text on NTP open.
  bool hide_aim_hint_text_on_ntp_open;

  // Whether to hide the other (non-AIM) page actions on NTP.
  bool hide_other_page_actions_on_ntp;

  // The maximum number of times the hint can be shown per day.
  int aim_hint_impression_limit_daily;

  // The maximum number of times the hint can be shown in total.
  int aim_hint_impression_limit_total;

  // Whether impression limits for the AIM hint are enabled.
  bool enable_hint_impression_limits;
};

// A config struct for features related to contextual search in omnibox.
struct ContextualSearch : Config<ContextualSearch> {
  ContextualSearch();
  ContextualSearch(const ContextualSearch&);
  ContextualSearch& operator=(const ContextualSearch&);
  ~ContextualSearch();

  DECLARE_FEATURE(kContextualSuggestionsAblateOthersWhenPresent);
  DECLARE_FEATURE(kOmniboxContextualSuggestions);
  DECLARE_FEATURE(kStarterPackPage);
  DECLARE_FEATURE(kContextualZeroSuggestLensFulfillment);
  DECLARE_FEATURE(kContextualSearchProviderAsyncSuggestInputs);
  DECLARE_FEATURE(kSendContextualUrlSuggestParam);
  DECLARE_FEATURE(kOmniboxContextualSearchOnFocusSuggestions);
  DECLARE_FEATURE(kContextualSearchBoxUsesContextualSearchProvider);
  DECLARE_FEATURE(kOmniboxZeroSuggestSynchronousMatchesOnly);
  DECLARE_FEATURE(kContextualSearchOpenLensActionUsesThumbnail);
  DECLARE_FEATURE(kSendPageTitleSuggestParam);
  DECLARE_FEATURE(kContextualSearchAlternativeActionLabel);
  DECLARE_FEATURE(kUseApcPaywallSignal);
  DECLARE_FEATURE(kShowSuggestionsOnNoApc);
  DECLARE_FEATURE(kOpenLensActionUITweaks);
  DECLARE_FEATURE(kSuggestionsFulfilledByLensSupported);
  DECLARE_FEATURE(kLoadingSuggestionsAnimation);

  // Whether to use contextual search features, for example the lens action.
  bool IsContextualSearchEnabled() const;

  // Whether to enable prefetching to support this feature's synchronous
  // match production requirement.
  bool IsEnabledWithPrefetch() const;

  // Whether to make contextual suggestions exclusive; that is, remove
  // other kinds of zero suggest matches when there are any contextual
  // search matches.
  bool contextual_suggestions_ablate_others_when_present;

  // Whether to restrict the ablation logic, triggered via
  // `contextual_suggestions_ablate_others_when_present`, such that we only
  // remove (non-contextual) search suggestions, instead of removing all
  // (non-contextual) zero suggest matches altogether.
  bool contextual_suggestions_ablate_search_only;

  // Whether to restrict the ablation logic, triggered via
  // `contextual_suggestions_ablate_others_when_present`, such that we only
  // remove URL suggestions, instead of removing all (non-contextual) zero
  // suggest matches altogether.
  bool contextual_suggestions_ablate_url_only;

  // Whether the starter pack page scope is enabled.
  bool starter_pack_page;

  // Enables fullfillment of contextual zero-prefix suggestions by delegating
  // the logic to Lens.
  bool contextual_zero_suggest_lens_fulfillment;

  // Controls async request handling in `ContextualSearchProvider`.
  bool csp_async_suggest_inputs;

  // This specifies the value for "ctxus" param on zero suggest requests,
  // and is left empty when that parameter is not to be included.
  std::string contextual_url_suggest_param;

  // Maximum number of contextual search suggestions for zero prefix suggest.
  size_t contextual_zps_limit;

  // Whether to use ContextualSearchProvider instead of ZeroSuggestProvider for
  // sourcing contextual search box matches.
  bool csb_uses_csp;

  // Whether to omit asynchronous matches in ZeroSuggestProvider specifically,
  // a behavior that is relevant to one planned configuration of this feature.
  bool zero_suggest_synchronous_matches_only;

  // Whether the Lens entrypoint action uses a thumbnail of web contents view
  // instead of its regular vector icon.
  bool open_lens_action_uses_thumbnail;

  // Whether to send the current page title (via "pageTitle" CGI param) on zero
  // suggest requests.
  // When set to false, the CGI param will not be sent at all (as opposed to
  // sending an empty value).
  bool send_page_title_suggest_param;

  // Which alternative action label to use for lens entrypoint action.
  int alternative_action_label;

  // Whether the Lens entrypoint action should be shown in the Omnibox popup.
  bool show_open_lens_action;

  // Whether to use the APC paywall signal to determine whether to show
  // contextual suggestions.
  bool use_apc_paywall_signal;

  // Whether to show contextual suggestions when the user focuses the omnibox
  // but APC is not yet available.
  bool show_suggestions_on_no_apc;

  // Whether to show the Lens entrypoint action with the new UI tweaks.
  bool open_lens_action_ui_tweaks;

  // Whether the feature to allow contextual search suggestions to be fulfilled
  // by Lens is supported. This allows contextual suggestions to open the Lens
  // overlay in the selection state. This is in contrast to the default behavior
  // where the suggestion is fulfilled by the contextual searchbox.
  bool suggestions_fulfilled_by_lens_supported;

  // Whether to enable the loading suggestions animation. This adds an
  // animation when contextual suggestions load into the omnibox popup.
  bool enable_loading_suggestions_animation;

  // The duration of the position animation when loading suggestions.
  int loading_suggestions_position_animation_duration;

  // The delay after the position animation begins that the opacity animation
  // should start after.
  int loading_suggestions_opacity_animation_delay;

  // The duration of the opacity animation when loading suggestions.
  int loading_suggestions_opacity_animation_duration;
};

// If enabled, allows MIA zero-prefix suggestions in NTP omnibox and realbox.
struct MiaZPS : Config<MiaZPS> {
  DECLARE_FEATURE(kOmniboxMiaZPS);

  MiaZPS();
  MiaZPS(const MiaZPS&);
  MiaZPS(MiaZPS&&);
  MiaZPS& operator=(const MiaZPS&);
  MiaZPS& operator=(MiaZPS&&);
  ~MiaZPS();
  bool enabled;
  // Whether to use non-normalized text for local history zp suggestions.
  bool local_history_non_normalized_contents;
  bool suppress_psuggest_backfill_with_mia;
};

// A config struct for the omnibox toolbelt.
struct Toolbelt : Config<Toolbelt> {
  DECLARE_FEATURE(kOmniboxToolbelt);

  Toolbelt();
  Toolbelt(const Toolbelt&);
  Toolbelt& operator=(const Toolbelt&);
  ~Toolbelt();

  // Whether the toolbelt is to be included in the omnibox.
  bool enabled;

  // Whether the toolbelt will be preserved after user types (after
  // input clears the zero suggest).
  bool keep_toolbelt_after_input;

  // Whether the toolbelt is preserved after entering keyword mode.
  bool keep_toolbelt_in_keyword_mode;

  // Whether the lens entrypoint action should stay unconditionally on the
  // toolbelt. When this is false, the regular triggering conditions apply
  // so the action can sometimes be included or sometimes not.
  bool always_include_lens_action;

  // Some of the variants we want to experiment with want a subset of actions.
  // For flexibility, may as well make all the actions finch params.

  // Disabling `show_lens_action` takes precedence over
  // `always_include_lens_action`.
  bool show_lens_action_on_non_ntp;
  bool show_lens_action_on_ntp;
  bool show_ai_mode_action_on_non_ntp;
  bool show_ai_mode_action_on_ntp;
  bool show_history_action_on_non_ntp;
  bool show_history_action_on_ntp;
  bool show_bookmarks_action_on_non_ntp;
  bool show_bookmarks_action_on_ntp;
  bool show_tabs_action_on_non_ntp;
  bool show_tabs_action_on_ntp;

  // Whether to rebuild button row views for the toolbelt without checking
  // the row's action count. This defaults to true and is just a kill switch
  // in case of an unexpected performance regression (safer to merge).
  bool rebuild_button_row_views;

  // Whether to set the location bar `OmniboxView` icon to the selected
  // toolbelt action's icon, when a toolbelt action is selected via keyboard.
  bool use_action_icons_in_location_bar;

  // Whether to select the toolbelt match before opening one of its actions.
  // This has the effect of taking the toolbelt match's fill into edit, the
  // verbatim user input text, instead of any text that might have been selected
  // or inline autocompleted previously.
  bool select_toolbelt_before_opening;
};

// If enabled, adjusts the indentation of the omnibox input and matches to fix
// the visual shift in omnibox input text when the omnibox popup opens.
struct AdjustOmniboxIndent : Config<AdjustOmniboxIndent> {
  DECLARE_FEATURE(kAdjustOmniboxIndent);

  AdjustOmniboxIndent();
  // Whether to indent the omnibox input text and icon when the popup is closed.
  bool indent_input_when_popup_closed;
  // Amount by which the indentation of the omnibox input icon is offset.
  int input_icon_indent_offset;
  // Amount by which the indentation of the omnibox input text is offset.
  int input_text_indent_offset;
  // Amount by which the indentation of the omnibox match icon is offset.
  int match_icon_indent_offset;
  // Amount by which the indentation of the omnibox match text is offset.
  int match_text_indent_offset;
};

// If enabled, allow document provider requests when all other conditions are
// met.
struct DocumentProvider : Config<DocumentProvider> {
  DocumentProvider();
  DocumentProvider(const DocumentProvider&);
  DocumentProvider(DocumentProvider&&);
  DocumentProvider& operator=(const DocumentProvider&);
  DocumentProvider& operator=(DocumentProvider&&);
  ~DocumentProvider();
  bool enabled;
  // The minimum input length required before requesting document suggestions.
  size_t min_query_length;
  // Whether to scope backoff state to the profile instead of the current
  // window.
  bool scope_backoff_to_profile;
  // How long to continue backing off from making new document suggestion
  // requests after receiving a backoff signal, when the backoff state is scoped
  // to the profile. If this is set to 0 (the default value) or a negative
  // value, the backoff state never resets. If this is set to a positive value,
  // the backoff state is reset after the specified amount of time. The value
  // can be supplied using --enable-features or in an experiment config using
  // the string representation expected by `base::TimeDeltaFromString()` (e.g.
  // "10m" or "12h"). Has no effect when `scope_backoff_to_profile` is false.
  base::TimeDelta backoff_duration;
};

// If enabled, pretends all matches are allowed to be default. This is very
// blunt, and needs refining before being launch ready. E.g. how does this
// affect transferred matches? This might cause crashes. This can result in
// misleading inline autocompletion; e.g. the bing.com favicon looks like the
// search loupe, so inlined bing results will like DSE search suggestions.
struct ForceAllowedToBeDefault : Config<ForceAllowedToBeDefault> {
  DECLARE_FEATURE(kForceAllowedToBeDefault);
  ForceAllowedToBeDefault();
  ForceAllowedToBeDefault(const ForceAllowedToBeDefault&);
  ForceAllowedToBeDefault(ForceAllowedToBeDefault&&);
  ForceAllowedToBeDefault& operator=(const ForceAllowedToBeDefault&);
  ForceAllowedToBeDefault& operator=(ForceAllowedToBeDefault&&);
  ~ForceAllowedToBeDefault();
  bool enabled;
};

// If enabled, NTP Realbox second column will allow displaying contextual and
// trending suggestions.
struct RealboxContextualAndTrendingSuggestions
    : Config<RealboxContextualAndTrendingSuggestions> {
  DECLARE_FEATURE(kRealboxContextualAndTrendingSuggestions);
  RealboxContextualAndTrendingSuggestions();
  RealboxContextualAndTrendingSuggestions(
      const RealboxContextualAndTrendingSuggestions&);
  RealboxContextualAndTrendingSuggestions(
      RealboxContextualAndTrendingSuggestions&&);
  RealboxContextualAndTrendingSuggestions& operator=(
      const RealboxContextualAndTrendingSuggestions&);
  RealboxContextualAndTrendingSuggestions& operator=(
      RealboxContextualAndTrendingSuggestions&&);
  ~RealboxContextualAndTrendingSuggestions();
  bool enabled;

  // The total number of matches a Section can contain across all Groups.
  size_t total_limit;
  // The total number of matches the `omnibox::GROUP_PREVIOUS_SEARCH_RELATED`
  // Group can contain.
  size_t contextual_suggestions_limit;
  // The total number of matches the `omnibox::GROUP_TRENDS` Group can contain.
  size_t trending_suggestions_limit;
};

// If enabled, injects a mock search engine using the same format as policy
// `EnterpriseSearchAggregatorSettings` to be applied. Ignored if feature
// policy is set.
struct SearchAggregatorProvider : Config<SearchAggregatorProvider> {
  DECLARE_FEATURE(kSearchAggregatorProvider);
  SearchAggregatorProvider();
  SearchAggregatorProvider(const SearchAggregatorProvider&);
  SearchAggregatorProvider& operator=(const SearchAggregatorProvider&);
  ~SearchAggregatorProvider();

  bool enabled;
  // Minimum length input must be to run the
  // `EnterpriseSearchAggregatorProvider`.
  int min_query_length;
  // If true, the response will be parsed in a utility process.
  bool parse_response_in_utility_process;
  // If true, the newer Discovery Engine OAuth scope will be used in suggestions
  // requests.
  bool use_discovery_engine_oauth_scope;
  // If true, doc provider won't run outside the drive scope. If false, doc
  // provider will run unscoped. Either way, doc provider won't run when in the
  // enterprise scope.
  bool disable_drive;
  // If true, the `EnterpriseSearchAggregatorSuggestionsService` will make
  // parallel requests for each type of suggestion.
  bool multiple_requests;
  // The specified value controls how `EnterpriseSearchAggregatorProvider` will
  // score suggestions. The following values are supported:
  //   "mixed": Use server-provided scores in scoped mode and client-calculated
  //     scores in unscoped mode. This is the default behavior and is also used
  //     if the supplied value is not one of the other options.
  //   "server": Use server-provided scores in both scoped and unscoped mode.
  //   "client": Use client-calculated scores in both scoped and unscoped mode.
  std::string relevance_scoring_mode;
  // If true, show unscoped `EnterpriseSearchAggregatorProvider` suggestions in
  // the NTP realbox.
  bool realbox_unscoped_suggestions;

  // See comments in enterprise_search_aggregator_provider.cc
  size_t scoring_max_matches_created_per_type;
  size_t scoring_max_scoped_matches_shown_per_type;
  size_t scoring_max_unscoped_matches_shown_per_type;
  size_t scoring_min_char_for_strong_text_match;
  size_t scoring_min_words_for_full_text_match_boost;
  int scoring_full_text_match_score;
  int scoring_score_per_strong_text_match;
  int scoring_score_per_weak_text_match;
  int scoring_max_text_score;
  int scoring_people_score_boost;
  int scoring_people_email_match_score_boost;
  bool scoring_prefer_contents_over_queries;
  size_t scoring_scoped_max_low_quality_matches;
  size_t scoring_unscoped_max_low_quality_matches;
  int scoring_low_quality_threshold;

 private:
  // Utility methods and members for setting up a mock search engine via Finch.
  // Restricted to `EnterpriseSearchManager` and its tests.
  friend class ::EnterpriseSearchManager;
  friend class ::EnterpriseSearchManagerProviderInjectionTest;
  friend class ::EnterpriseSearchManagerRequireShortcutTest;
  FRIEND_TEST_ALL_PREFIXES(::EnterpriseSearchManagerProviderInjectionTest,
                           Verify);

  bool AreMockEnginesValid() const;
  std::vector<base::Value> CreateMockSearchEngines() const;
  base::Value::Dict CreateMockSearchAggregator(bool featured_by_policy) const;

  // The search engine name, shown in the Omnibox.
  std::string name;
  // The shortcut the user enters to trigger the search.
  std::string shortcut;
  // The URL on which to perform a search.
  std::string search_url;
  // The URL that provides search suggestions.
  std::string suggest_url;
  // The URL to an imanage that will be used on search suggestions.
  std::string icon_url;
  // If enabled, Chrome will blend search suggestions with other Omnibox
  // suggestions without requiring keyword mode.
  bool require_shortcut;
};

// If enabled, uses RichAnswerTemplate instead of SuggestionAnswer to display
// answers.
struct SuggestionAnswerMigration : Config<SuggestionAnswerMigration> {
  DECLARE_FEATURE(kOmniboxSuggestionAnswerMigration);
  SuggestionAnswerMigration();
  SuggestionAnswerMigration(const SuggestionAnswerMigration&);
  SuggestionAnswerMigration(SuggestionAnswerMigration&&);
  SuggestionAnswerMigration& operator=(const SuggestionAnswerMigration&);
  SuggestionAnswerMigration& operator=(SuggestionAnswerMigration&&);
  ~SuggestionAnswerMigration();
  bool enabled;
};

struct OmniboxZpsSuggestionLimit : Config<OmniboxZpsSuggestionLimit> {
  DECLARE_FEATURE(kOmniboxZpsSuggestionLimit);
  OmniboxZpsSuggestionLimit();
  OmniboxZpsSuggestionLimit(const OmniboxZpsSuggestionLimit&);
  OmniboxZpsSuggestionLimit(OmniboxZpsSuggestionLimit&&);
  OmniboxZpsSuggestionLimit& operator=(const OmniboxZpsSuggestionLimit&);
  OmniboxZpsSuggestionLimit& operator=(OmniboxZpsSuggestionLimit&&);
  ~OmniboxZpsSuggestionLimit();
  bool enabled;
  // Max number of zps suggestions to show.
  size_t max_suggestions;
  // Max number of search zps suggestions to show.
  size_t max_search_suggestions;
  // Max number of url zps suggestions to show.
  size_t max_url_suggestions;
};

// Enables url suggestions when omnibox is focused on Web/SRP.
struct OmniboxUrlSuggestionsOnFocus : Config<OmniboxUrlSuggestionsOnFocus> {
  DECLARE_FEATURE(kOmniboxUrlSuggestionsOnFocus);
  OmniboxUrlSuggestionsOnFocus();
  OmniboxUrlSuggestionsOnFocus(const OmniboxUrlSuggestionsOnFocus&);
  OmniboxUrlSuggestionsOnFocus& operator=(const OmniboxUrlSuggestionsOnFocus&);
  ~OmniboxUrlSuggestionsOnFocus();
  bool enabled;
  bool show_recently_closed_tabs;
  // Number of days to consider for most visited sites (0-indexed).
  size_t most_visited_recency_window;
  // Recency factor heuristic used to calculate most visited sites.  Must be
  // one of `kMvtScoringParamRecencyFactor` feature param values listed in
  // //components/history/core/browser/features.h.
  std::string most_visited_recency_factor;
  // Whether MostVisitedProvider should directly query HistoryService for its
  // most visited sites list. False if the provider should query TopSites.
  bool directly_query_history_service;
  // Whether to prefetch the most visited sites.
  bool prefetch_most_visited_sites;
  // The debouncing delay (in milliseconds) to use when throttling
  // HistoryService requests.
  int prefetch_most_visited_sites_delay_ms;
  // Max number of URLs that will be requested from history.
  size_t max_requested_urls_from_history;

  bool MostVisitedPrefetchingEnabled() const;
};

// Enables the HaTS for On-Focus ZPS so that it may show up for a user.
struct HappinessTrackingSurveyForOmniboxOnFocusZps
    : Config<HappinessTrackingSurveyForOmniboxOnFocusZps> {
  DECLARE_FEATURE(kHappinessTrackingSurveyForOmniboxOnFocusZps);
  HappinessTrackingSurveyForOmniboxOnFocusZps();
  HappinessTrackingSurveyForOmniboxOnFocusZps(
      const HappinessTrackingSurveyForOmniboxOnFocusZps&);
  HappinessTrackingSurveyForOmniboxOnFocusZps(
      HappinessTrackingSurveyForOmniboxOnFocusZps&&);
  HappinessTrackingSurveyForOmniboxOnFocusZps& operator=(
      const HappinessTrackingSurveyForOmniboxOnFocusZps&);
  HappinessTrackingSurveyForOmniboxOnFocusZps& operator=(
      HappinessTrackingSurveyForOmniboxOnFocusZps&&);
  ~HappinessTrackingSurveyForOmniboxOnFocusZps();
  bool enabled;
  // Number of times the omnibox must be focused on SRP/Web before the survey
  // may be shown
  size_t focus_threshold;
  // Number of ms before the survey may be shown.
  size_t survey_delay;
  // Trigger ID of Intent and Satisfaction survey.
  std::string happiness_trigger_id;
  // Trigger ID of Usefulness and Distraction survey.
  std::string utility_trigger_id;
};

struct ComposeboxSuggestionLimit : Config<ComposeboxSuggestionLimit> {
  DECLARE_FEATURE(kComposeboxSuggestionLimit);
  ComposeboxSuggestionLimit();
  ComposeboxSuggestionLimit(const ComposeboxSuggestionLimit&);
  ComposeboxSuggestionLimit(ComposeboxSuggestionLimit&&);
  ComposeboxSuggestionLimit& operator=(const ComposeboxSuggestionLimit&);
  ComposeboxSuggestionLimit& operator=(ComposeboxSuggestionLimit&&);
  ~ComposeboxSuggestionLimit();
  bool enabled;
  // Max number of zps suggestions to show.
  size_t max_suggestions;
  // Max number of aim zps suggestions to show.
  size_t max_aim_suggestions;
  // Max number of contextual zps suggestions to show.
  size_t max_contextual_suggestions;
};

// Do not add new configs here at the bottom by default. They should be ordered
// alphabetically.

#undef DECLARE_FEATURE

}  // namespace omnibox_feature_configs

#endif  // COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURE_CONFIGS_H_
