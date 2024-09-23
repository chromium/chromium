// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/config.h"

#include "base/test/scoped_feature_list.h"
#include "components/history_clusters/core/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {

using ::testing::ElementsAre;

TEST(HistoryClustersConfigTest, LocaleOrLanguageAllowlistDefault) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(internal::kJourneys);

  const struct {
    const std::string locale;
    bool expected_is_journeys_enabled;
  } kLocaleTestCases[] = {{"", false},
                          {"en", true},
                          {"fr", true},
                          {"zh-TW", false},
                          {" random junk ", false}};

  for (const auto& test : kLocaleTestCases) {
    EXPECT_EQ(test.expected_is_journeys_enabled,
              IsApplicationLocaleSupportedByJourneys(test.locale))
        << test.locale;
  }
}

TEST(HistoryClustersConfigTest, LocaleOrLanguageWildcard) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{
          internal::kJourneys,
          {{"JourneysLocaleOrLanguageAllowlist", "*"}},
      }},
      {});

  EXPECT_TRUE(IsApplicationLocaleSupportedByJourneys(""));
  EXPECT_TRUE(IsApplicationLocaleSupportedByJourneys("*"));
  EXPECT_TRUE(IsApplicationLocaleSupportedByJourneys("en"));
  EXPECT_TRUE(IsApplicationLocaleSupportedByJourneys("zh-TW"));
  EXPECT_TRUE(IsApplicationLocaleSupportedByJourneys("random junk"));
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
       {internal::kOmniboxAction, {}}},
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
    EXPECT_EQ(test.expected_is_journeys_enabled,
              IsApplicationLocaleSupportedByJourneys(test.locale))
        << test.locale;
  }
}

}  // namespace history_clusters
