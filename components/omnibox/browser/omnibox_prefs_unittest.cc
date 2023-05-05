// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_prefs.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/groups.pb.h"

namespace omnibox {

class OmniboxPrefsTest : public ::testing::Test {
 public:
  OmniboxPrefsTest() = default;
  OmniboxPrefsTest(const OmniboxPrefsTest&) = delete;
  OmniboxPrefsTest& operator=(const OmniboxPrefsTest&) = delete;

  void SetUp() override { RegisterProfilePrefs(GetPrefs()->registry()); }

  TestingPrefServiceSimple* GetPrefs() { return &pref_service_; }

  base::HistogramTester* histogram() { return &histogram_; }

 private:
  TestingPrefServiceSimple pref_service_;
  base::HistogramTester histogram_;
};

TEST_F(OmniboxPrefsTest, ToggleSuggestionGroupId) {
  {
    // Expect `UMAGroupId::kTrends` to be in the default state.
    EXPECT_EQ(SuggestionGroupVisibility::DEFAULT,
              GetUserPreferenceForSuggestionGroupVisibility(GetPrefs(),
                                                            GROUP_TRENDS));
    histogram()->ExpectTotalCount(kGroupIdToggledOffHistogram, 0);

    // Expect `UMAGroupId::kTrendsEntityChips` to be in the default state.
    EXPECT_EQ(SuggestionGroupVisibility::DEFAULT,
              GetUserPreferenceForSuggestionGroupVisibility(
                  GetPrefs(), GROUP_TRENDS_ENTITY_CHIPS));
    histogram()->ExpectTotalCount(kGroupIdToggledOnHistogram, 0);
  }
  {
    SetUserPreferenceForSuggestionGroupVisibility(
        GetPrefs(), GROUP_TRENDS, SuggestionGroupVisibility::HIDDEN);

    // Expect `UMAGroupId::kTrends` to have been toggled hidden.
    EXPECT_EQ(SuggestionGroupVisibility::HIDDEN,
              GetUserPreferenceForSuggestionGroupVisibility(GetPrefs(),
                                                            GROUP_TRENDS));
    histogram()->ExpectTotalCount(kGroupIdToggledOffHistogram, 1);
    histogram()->ExpectBucketCount(kGroupIdToggledOffHistogram,
                                   UMAGroupId::kTrends, 1);

    // Expect `UMAGroupId::kTrendsEntityChips` to have remained in the default
    // state.
    EXPECT_EQ(SuggestionGroupVisibility::DEFAULT,
              GetUserPreferenceForSuggestionGroupVisibility(
                  GetPrefs(), GROUP_TRENDS_ENTITY_CHIPS));
    histogram()->ExpectTotalCount(kGroupIdToggledOnHistogram, 0);
  }
  {
    SetUserPreferenceForSuggestionGroupVisibility(
        GetPrefs(), GROUP_TRENDS, SuggestionGroupVisibility::SHOWN);
    SetUserPreferenceForSuggestionGroupVisibility(
        GetPrefs(), GROUP_TRENDS_ENTITY_CHIPS,
        SuggestionGroupVisibility::HIDDEN);

    // Expect `UMAGroupId::kTrends` to have been toggled visible again.
    EXPECT_EQ(SuggestionGroupVisibility::SHOWN,
              GetUserPreferenceForSuggestionGroupVisibility(GetPrefs(),
                                                            GROUP_TRENDS));
    histogram()->ExpectTotalCount(kGroupIdToggledOnHistogram, 1);
    histogram()->ExpectBucketCount(kGroupIdToggledOnHistogram,
                                   UMAGroupId::kTrends, 1);

    // Expect `UMAGroupId::kTrendsEntityChips` to have been toggled hidden.
    EXPECT_EQ(SuggestionGroupVisibility::HIDDEN,
              GetUserPreferenceForSuggestionGroupVisibility(
                  GetPrefs(), GROUP_TRENDS_ENTITY_CHIPS));
    histogram()->ExpectTotalCount(kGroupIdToggledOffHistogram, 2);
    histogram()->ExpectBucketCount(kGroupIdToggledOffHistogram,
                                   UMAGroupId::kTrendsEntityChips, 1);
  }
}

}  // namespace omnibox
