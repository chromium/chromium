// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_prefs.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace omnibox {

class OmniboxPrefsTest : public ::testing::Test {
 public:
  OmniboxPrefsTest() = default;
  OmniboxPrefsTest(const OmniboxPrefsTest&) = delete;
  OmniboxPrefsTest& operator=(const OmniboxPrefsTest&) = delete;

  void SetUp() override {
    omnibox::RegisterProfilePrefs(GetPrefs()->registry());
  }

  TestingPrefServiceSimple* GetPrefs() { return &pref_service_; }

  base::HistogramTester* histogram() { return &histogram_; }

 private:
  TestingPrefServiceSimple pref_service_;
  base::HistogramTester histogram_;
};

TEST_F(OmniboxPrefsTest, SuggestionGroupId) {
  const int kOnboardingGroupId = 40001;
  const int kRZPSGroupId = 40009;
  {
    // Expect |kOnboardingGroupId| to be in the default state.
    EXPECT_EQ(SuggestionGroupVisibility::DEFAULT,
              GetUserPreferenceForSuggestionGroupVisibility(
                  GetPrefs(), kOnboardingGroupId));
    histogram()->ExpectTotalCount(kToggleSuggestionGroupIdOffHistogram, 0);

    // Expect |kRZPSGroupId| to be in the default state.
    EXPECT_EQ(SuggestionGroupVisibility::DEFAULT,
              GetUserPreferenceForSuggestionGroupVisibility(GetPrefs(),
                                                            kRZPSGroupId));
    histogram()->ExpectTotalCount(kToggleSuggestionGroupIdOnHistogram, 0);
  }
  {
    SetUserPreferenceForSuggestionGroupVisibility(
        GetPrefs(), kOnboardingGroupId, SuggestionGroupVisibility::HIDDEN);

    // Expect |kOnboardingGroupId| to have been toggled hidden.
    EXPECT_EQ(SuggestionGroupVisibility::HIDDEN,
              GetUserPreferenceForSuggestionGroupVisibility(
                  GetPrefs(), kOnboardingGroupId));
    histogram()->ExpectTotalCount(kToggleSuggestionGroupIdOffHistogram, 1);
    histogram()->ExpectBucketCount(kToggleSuggestionGroupIdOffHistogram,
                                   kOnboardingGroupId, 1);

    // Expect |kRZPSGroupId| to have remained in the default state.
    EXPECT_EQ(SuggestionGroupVisibility::DEFAULT,
              GetUserPreferenceForSuggestionGroupVisibility(GetPrefs(),
                                                            kRZPSGroupId));
    histogram()->ExpectTotalCount(kToggleSuggestionGroupIdOnHistogram, 0);
  }
  {
    SetUserPreferenceForSuggestionGroupVisibility(
        GetPrefs(), kOnboardingGroupId, SuggestionGroupVisibility::SHOWN);
    SetUserPreferenceForSuggestionGroupVisibility(
        GetPrefs(), kRZPSGroupId, SuggestionGroupVisibility::HIDDEN);

    // Expect |kOnboardingGroupId| to have been toggled visible again.
    EXPECT_EQ(SuggestionGroupVisibility::SHOWN,
              GetUserPreferenceForSuggestionGroupVisibility(
                  GetPrefs(), kOnboardingGroupId));
    histogram()->ExpectTotalCount(kToggleSuggestionGroupIdOnHistogram, 1);
    histogram()->ExpectBucketCount(kToggleSuggestionGroupIdOnHistogram,
                                   kOnboardingGroupId, 1);

    // Expect |kRZPSGroupId| to have been toggled hidden.
    EXPECT_EQ(SuggestionGroupVisibility::HIDDEN,
              GetUserPreferenceForSuggestionGroupVisibility(GetPrefs(),
                                                            kRZPSGroupId));
    histogram()->ExpectTotalCount(kToggleSuggestionGroupIdOffHistogram, 2);
    histogram()->ExpectBucketCount(kToggleSuggestionGroupIdOffHistogram,
                                   kRZPSGroupId, 1);
  }
}

}  // namespace omnibox
