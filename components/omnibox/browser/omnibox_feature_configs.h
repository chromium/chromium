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

  struct MyFeature {
    DECLARE_FEATURE(kMyFeature);

    MyFeature();
    static const MyFeature& Get();

    bool enabled;
    int my_param;
  }

  // omnibox_feature_configs.cc

  // static
  BASE_FEATURE(MyFeature::kMyFeature, "OmniboxMyFeature",
               base::FEATURE_DISABLED_BY_DEFAULT);

  MyFeature::MyFeature() {
    enabled = base::FeatureList::IsEnabled(omnibox::kMyFeature);
    my_param =
        base::FeatureParam<int>(&omnibox::kMyFeature, "my_param", 0).Get();
  }

  // static
  const MyFeature& MyFeature::Get() {
    static MyFeature config;
    return config;
  }


(2) Use the config:

  int x = omnibox_feature_configs::MyFeature::Get().my_param;


(3) Override the config in tests:

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      omnibox::kMyFeature, {{"my_param", "1"}});
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::MyFeature> scoped_config;

  scoped_feature_list.Reset();
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      omnibox::kMyFeature, {{"my_param", "2"}});
  scoped_config.Reset();
*/

// A substitute for `BASE_DECLARE_FEATURE` for nesting in structs.
#define DECLARE_FEATURE(feature) static CONSTINIT const base::Feature feature

// Util for overriding configs in tests. `T` must have a `static const T& Get()`
// method.
template <class T>
class ScopedConfigForTesting {
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

// If enabled, the shortcut provider is more aggressive in scoring.
struct ShortcutBoosting {
  DECLARE_FEATURE(kShortcutBoost);
  ShortcutBoosting();
  static const ShortcutBoosting& Get();
  bool enabled;
  // The scores to use for boosting search and URL suggestions respectively.
  // Setting to 0 will prevent boosting.
  int search_score;
  int url_score;
  bool counterfactual;
};

}  // namespace omnibox_feature_configs

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_FEATURE_CONFIGS_H_
