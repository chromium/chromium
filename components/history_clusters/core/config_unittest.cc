// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/config.h"

#include "base/test/scoped_feature_list.h"
#include "components/history_clusters/core/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {

TEST(HistoryClustersConfigTest, PlainEnabled) {
  const struct {
    const std::string locale;
    bool expected_is_journeys_enabled;
  } kLocaleTestCases[] = {{"", false},
                          {"en", false},
                          {"fr", false},
                          {"zh-TW", false},
                          {" random junk ", false}};

  for (const auto& test : kLocaleTestCases) {
    ResetConfigForTesting();
    OverrideWithFinch(test.locale);

    EXPECT_EQ(test.expected_is_journeys_enabled,
              GetConfig().is_journeys_enabled)
        << test.locale;
  }
}

TEST(HistoryClustersConfigTest, OmniboxAction) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({internal::kJourneys, kOmniboxAction}, {});

  const struct {
    const std::string locale;
    bool expected_is_journeys_enabled;
  } kLocaleTestCases[] = {{"", true},
                          {"en", true},
                          {"fr", true},
                          {"zh-TW", true},
                          {" random junk ", true}};

  for (const auto& test : kLocaleTestCases) {
    ResetConfigForTesting();
    OverrideWithFinch(test.locale);

    EXPECT_EQ(test.expected_is_journeys_enabled,
              GetConfig().is_journeys_enabled)
        << test.locale;
  }
}

TEST(HistoryClustersConfigTest, LocaleOrLanguageAllowlist) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{
           internal::kJourneys,
           // Test that we're tolerant of spaces, colons, whole locales, as well
           // as primary language subcodes.
           {{"JourneysLocaleOrLanguageAllowlist", "en, fr:de:zh-TW"}},
       },
       {kOmniboxAction, {}}},
      {});

  const struct {
    const std::string locale;
    bool expected_is_journeys_enabled;
  } kLocaleTestCases[] = {{"", false},
                          {"en", true},
                          {"en-US", true},
                          {"fr", true},
                          {" random junk ", false},
                          {"de", true},
                          {"el", false},
                          {"zh-TW", true},
                          {"zh", false},
                          {"zh-CN", false}};

  for (const auto& test : kLocaleTestCases) {
    ResetConfigForTesting();

    OverrideWithFinch(test.locale);

    EXPECT_EQ(test.expected_is_journeys_enabled,
              GetConfig().is_journeys_enabled)
        << test.locale;
  }
}

}  // namespace history_clusters
