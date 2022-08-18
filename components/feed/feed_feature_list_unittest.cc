
// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/feed_feature_list.h"
#include <sstream>

#include "base/callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/signin/public/base/consent_level.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/sync/base/features.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace feed {
namespace {

// Build every possible arrangement of `features` into enabled and disabled
// groups and call `test()`.
void ForAllCombinationsOfFeatures(
    std::vector<base::Feature> features,
    base::RepeatingCallback<void(std::vector<base::Feature>,
                                 std::vector<base::Feature>)> test) {
  int num_cases = std::pow(2, features.size());
  for (int case_i = 0; case_i < num_cases; ++case_i) {
    std::vector<base::Feature> enabled, disabled;
    for (size_t feature_j = 0; feature_j < features.size(); ++feature_j) {
      if ((case_i >> feature_j) % 2)
        enabled.push_back(features[feature_j]);
      else
        disabled.push_back(features[feature_j]);
    }
    test.Run(std::move(enabled), std::move(disabled));
  }
}

std::string FeaturesToString(const std::vector<base::Feature>& features) {
  std::ostringstream oss;
  for (auto& feature : features)
    oss << feature.name << ',';
  return oss.str();
}

}  // namespace

TEST(FeedFeatureList, GetConsentLevelNeededForPersonalizedFeed) {
  // Test all combinations of features.
  ForAllCombinationsOfFeatures(
      {
        kPersonalizeFeedNonSyncUsers,
#if BUILDFLAG(IS_ANDROID)
            syncer::kSyncAndroidPromosWithTitle,
#endif  // BUILDFLAG(IS_ANDROID)
      },
      base::BindRepeating([](std::vector<base::Feature> enabled,
                             std::vector<base::Feature> disabled) {
        base::test::ScopedFeatureList scoped_feature_list;
        scoped_feature_list.InitWithFeatures(enabled, disabled);

        // Should return kSignin only when all features are enabled.
        signin::ConsentLevel expected_consent_level =
            signin::ConsentLevel::kSync;
        if (disabled.empty())
          expected_consent_level = signin::ConsentLevel::kSignin;

        EXPECT_EQ(expected_consent_level,
                  GetConsentLevelNeededForPersonalizedFeed())
            << "Wrong consent level when features enabled="
            << FeaturesToString(enabled)
            << " disabled=" << FeaturesToString(disabled);
      }));
}

}  // namespace feed