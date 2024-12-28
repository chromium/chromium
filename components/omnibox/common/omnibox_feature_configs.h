// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURE_CONFIGS_H_
#define COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURE_CONFIGS_H_

#include "base/feature_list.h"
#include "base/time/time.h"
#include "base/values.h"

class EnterpriseSearchManagerProviderInjectionTest;

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

// If enabled, injects a mock search engine using the same format as policy
// `EnterpriseSearchAggregatorSettings` to be applied. Ignored if feature
// policy is set.
class SearchAggregatorProvider : public Config<SearchAggregatorProvider> {
  DECLARE_FEATURE(kSearchAggregatorProvider);

 public:
  SearchAggregatorProvider();
  SearchAggregatorProvider(const SearchAggregatorProvider&);
  SearchAggregatorProvider& operator=(const SearchAggregatorProvider&);
  ~SearchAggregatorProvider();

  bool enabled() const { return enabled_; }
  bool valid_search_engine() const { return valid_search_engine_; }
  std::vector<base::Value> GetSearchEngines() const;
  bool trigger_omnibox_blending() const { return trigger_omnibox_blending_; }

 private:
  friend ::EnterpriseSearchManagerProviderInjectionTest;

  // Makes it easier for tests to set a config.
  void Init(bool enabled,
            const std::string& name,
            const std::string& shortcut,
            const std::string& search_url,
            const std::string& suggest_url,
            const std::string& icon_url,
            bool trigger_omnibox_blending);
  // Same as `Init(,,,,,,)` setting all string arguments as empty.
  void Init(bool enabled, bool trigger_omnibox_blending);

  // Returns a dictionary corresponding to the search engine
  base::Value::Dict CreateMockSearchAggregator(bool featured_by_policy) const;

  // If true, injects mock search aggregator in the Omnibox.
  bool enabled_ = false;
  // If true, the data passes soft validation that prevents crashes downstream.
  // Only set as true is `enabled_` is true.
  bool valid_search_engine_ = false;
  // The search engine name, shown in the Omnibox.
  std::string name_;
  // The shortcut the user enters to trigger the search.
  std::string shortcut_;
  // The URL on which to perform a search.
  std::string search_url_;
  // The URL that provides search suggestions.
  std::string suggest_url_;
  // The URL to an imanage that will be used on search suggestions.
  std::string icon_url_;
  // If enabled, Chrome will blend search suggestions with other Omnibox
  // suggestions without requiring keyword mode.
  bool trigger_omnibox_blending_ = false;
};

// If enabled, uses RichAnswerTemplate instead of SuggestionAnswer to display
// answers.
struct SuggestionAnswerMigration : Config<SuggestionAnswerMigration> {
  DECLARE_FEATURE(kOmniboxSuggestionAnswerMigration);
  SuggestionAnswerMigration();
  bool enabled;
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

#endif  // COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FEATURE_CONFIGS_H_
