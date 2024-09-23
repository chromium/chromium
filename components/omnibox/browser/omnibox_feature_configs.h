// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_FEATURE_CONFIGS_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_FEATURE_CONFIGS_H_

#include "base/feature_list.h"

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
    static T config;
    return config;
  }
};

// Util for overriding configs in tests.
template <class T>
class ScopedConfigForTesting : Config<T> {
 public:
  ScopedConfigForTesting() : original_config_(Get()) { Reset(); }
  ScopedConfigForTesting(const ScopedConfigForTesting&) = delete;
  ScopedConfigForTesting& operator=(const ScopedConfigForTesting&) = delete;
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

// If enabled, allow document provider requests when all other conditions are
// met.
struct DocumentProvider : Config<DocumentProvider> {
  DocumentProvider();
  bool enabled;
  // The minimum input length required before requesting document suggestions.
  size_t min_query_length;
  // Whether to ignore the state of the document provider when deciding to
  // finish debouncing.
  bool ignore_when_debouncing;
  // Whether to treat an HTTP 401 response code as a backoff signal.
  bool backoff_on_401;
};

// If enabled, pretends all matches are allowed to be default. This is very
// blunt, and needs refining before being launch ready. E.g. how does this
// affect transferred matches? This might cause crashes. This can result in
// misleading inline autocompletion; e.g. the bing.com favicon looks like the
// search loupe, so inlined bing results will like DSE search suggestions.
struct ForceAllowedToBeDefault : Config<ForceAllowedToBeDefault> {
  DECLARE_FEATURE(kForceAllowedToBeDefault);
  ForceAllowedToBeDefault();
  bool enabled;
};

// If enabled, NTP Realbox second column will allow displaying contextual and
// trending suggestions.
struct RealboxContextualAndTrendingSuggestions
    : Config<RealboxContextualAndTrendingSuggestions> {
  DECLARE_FEATURE(kRealboxContextualAndTrendingSuggestions);
  RealboxContextualAndTrendingSuggestions();
  bool enabled;

  // The total number of matches a Section can contain across all Groups.
  size_t total_limit;
  // The total number of matches the `omnibox::GROUP_PREVIOUS_SEARCH_RELATED`
  // Group can contain.
  size_t contextual_suggestions_limit;
  // The total number of matches the `omnibox::GROUP_TRENDS` Group can contain.
  size_t trending_suggestions_limit;
};

// If enabled, omnibox reports the number of zero-prefix suggestions shown in
// the session which ends when autocomplete clears the set of results. The
// current behavior incorrectly reports the number of zero-prefix suggestions in
// the last set of results, which would be 0 for non-zps queries.
struct ReportNumZPSInSession : Config<ReportNumZPSInSession> {
  DECLARE_FEATURE(kReportNumZPSInSession);
  ReportNumZPSInSession();
  bool enabled;
};

// If enabled, uses RichAnswerTemplate instead of SuggestionAnswer to display
// answers.
struct SuggestionAnswerMigration : Config<SuggestionAnswerMigration> {
  DECLARE_FEATURE(kOmniboxSuggestionAnswerMigration);
  SuggestionAnswerMigration();
  bool enabled;
};

// If enabled, the shortcut provider is more aggressive in scoring.
struct ShortcutBoosting : Config<ShortcutBoosting> {
  DECLARE_FEATURE(kShortcutBoost);
  ShortcutBoosting();
  bool enabled;
  // The scores to use for boosting search and URL suggestions respectively.
  // Setting to 0 will prevent boosting.
  int search_score;
  int url_score;
  bool counterfactual;
  // Shortcuts are boosted if either:
  // 1) They are the top shortcut.
  // 2) OR they have more hits than `non_top_hit_[searches_]threshold`. If this
  //    is 1, then all shortcuts are boosted, since all have at least 1 hit. If
  //    0 (default), then no shortcuts will be boosted through (2) - only the
  //    top shortcut will be boosted.
  int non_top_hit_threshold;
  int non_top_hit_search_threshold;
  // If enabled, boosted shortcuts will be grouped with searches. Unboosted
  // shortcuts are grouped with URLs, like traditionally, regardless of
  // `group_with_searches`.
  bool group_with_searches;
};

// If enabled, affects autocompleted keywords (e.g. input 'youtu Ispiryan' ->
// match 'Ispiryan - Search YouTube').
// 1) These autocompleted keywords will be scored `score` instead of the default
//    450.
// 2) Autocompletes keyword even when the full keyword is typed ('youtube.com').
//    Otherwise, only incomplete keywords ('youtube.co') are autocompleted.
struct VitalizeAutocompletedKeywords : Config<VitalizeAutocompletedKeywords> {
  DECLARE_FEATURE(kVitalizeAutocompletedKeywords);
  VitalizeAutocompletedKeywords();
  bool enabled;
  // Should probably be less than 1100; i.e. the score for complete keywords
  // in `SearchProvider::CalculateRelevanceForKeywordVerbatim()`. Otherwise, it
  // would be weird if the input 'youtube.co Ispiryan' produces a higher scored
  // keyword match than 'youtube.com Ispiryan'.
  int score;
};

// Do not add new configs here at the bottom by default. They should be ordered
// alphabetically.

#undef DECLARE_FEATURE

}  // namespace omnibox_feature_configs

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_FEATURE_CONFIGS_H_
