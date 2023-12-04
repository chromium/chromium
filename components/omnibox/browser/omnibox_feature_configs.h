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

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      omnibox_feature_configs::MyFeature::kMyFeature, {{"my_param", "1"}});
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::MyFeature> scoped_config;

  scoped_feature_list.Reset();
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      omnibox_feature_configs::MyFeature::kMyFeature, {{"my_param", "2"}});
  scoped_config.Reset();
*/

// A substitute for `BASE_DECLARE_FEATURE` for nesting in structs.
#define DECLARE_FEATURE(feature) static CONSTINIT const base::Feature feature

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
  ScopedConfigForTesting() : original_config_(T::Get()) { Reset(); }
  ScopedConfigForTesting(const ScopedConfigForTesting&) = delete;
  ScopedConfigForTesting& operator=(const ScopedConfigForTesting&) = delete;
  ~ScopedConfigForTesting() { const_cast<T&>(T::Get()) = original_config_; }
  void Reset() { const_cast<T&>(T::Get()) = {}; }

 private:
  T original_config_;
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

#undef DECLARE_FEATURE

}  // namespace omnibox_feature_configs

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_FEATURE_CONFIGS_H_
