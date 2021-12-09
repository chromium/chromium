// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/features.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {

TEST(HistoryClustersFeaturesTest, PlainEnabled) {
  EXPECT_FALSE(IsJourneysEnabled(""));
  EXPECT_FALSE(IsJourneysEnabled("en"));
  EXPECT_FALSE(IsJourneysEnabled("fr"));
  EXPECT_FALSE(IsJourneysEnabled("zh-TW"));
  EXPECT_FALSE(IsJourneysEnabled(" random junk "));

  base::test::ScopedFeatureList features;
  features.InitWithFeatures({internal::kJourneys, kOmniboxAction}, {});

  EXPECT_TRUE(IsJourneysEnabled(""));
  EXPECT_TRUE(IsJourneysEnabled("en"));
  EXPECT_TRUE(IsJourneysEnabled("fr"));
  EXPECT_TRUE(IsJourneysEnabled("zh-TW"));
  EXPECT_TRUE(IsJourneysEnabled(" random junk "));
}

TEST(HistoryClustersFeaturesTest, LocaleOrLanguageAllowlist) {
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

  EXPECT_FALSE(IsJourneysEnabled(""));
  EXPECT_TRUE(IsJourneysEnabled("en"));
  EXPECT_TRUE(IsJourneysEnabled("en-US"));
  EXPECT_TRUE(IsJourneysEnabled("fr"));
  EXPECT_FALSE(IsJourneysEnabled(" random junk "));
  EXPECT_TRUE(IsJourneysEnabled("de"));
  EXPECT_FALSE(IsJourneysEnabled("el"));
  EXPECT_TRUE(IsJourneysEnabled("zh-TW"));
  EXPECT_FALSE(IsJourneysEnabled("zh"));
  EXPECT_FALSE(IsJourneysEnabled("zh-CN"));
}

}  // namespace history_clusters
