// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_triggered_feature_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "testing/gtest/include/gtest/gtest.h"

class OmniboxTriggeredFeatureServiceTest : public testing::Test {
 public:
  OmniboxTriggeredFeatureServiceTest() = default;

  ~OmniboxTriggeredFeatureServiceTest() override = default;

  OmniboxTriggeredFeatureService service_;
  OmniboxTriggeredFeatureService::Features feature_triggered_in_session_;
  base::HistogramTester histogram_;
};

TEST_F(OmniboxTriggeredFeatureServiceTest, NoFeaturesTriggered) {
  service_.RecordToLogs(&feature_triggered_in_session_);

  EXPECT_TRUE(feature_triggered_in_session_.empty());

  histogram_.ExpectTotalCount("Omnibox.RichAutocompletion.Triggered", 0);
  histogram_.ExpectUniqueSample("Omnibox.RichAutocompletion.Triggered.Any",
                                false, 1);
}

TEST_F(OmniboxTriggeredFeatureServiceTest, TwoFeaturesTriggered) {
  service_.FeatureTriggered(
      OmniboxTriggeredFeatureService::Feature::kBookmarkPaths);
  service_.FeatureTriggered(OmniboxTriggeredFeatureService::Feature::
                                kShortBookmarkSuggestionsByTotalInputLength);
  service_.RecordToLogs(&feature_triggered_in_session_);

  EXPECT_THAT(feature_triggered_in_session_,
              testing::ElementsAre(
                  OmniboxTriggeredFeatureService::Feature::kBookmarkPaths,
                  OmniboxTriggeredFeatureService::Feature::
                      kShortBookmarkSuggestionsByTotalInputLength));

  histogram_.ExpectTotalCount("Omnibox.RichAutocompletion.Triggered", 0);
  histogram_.ExpectUniqueSample("Omnibox.RichAutocompletion.Triggered.Any",
                                false, 1);

  service_.ResetSession();
  service_.RecordToLogs(&feature_triggered_in_session_);
  EXPECT_TRUE(feature_triggered_in_session_.empty());
}

TEST_F(OmniboxTriggeredFeatureServiceTest, TriggerRichAutocompletionType_kNo) {
  // Simulate 2 updates in the session, neither of which had rich
  // autocompletion.
  service_.RichAutocompletionTypeTriggered(
      AutocompleteMatch::RichAutocompletionType::kNone);
  service_.RichAutocompletionTypeTriggered(
      AutocompleteMatch::RichAutocompletionType::kNone);

  service_.RecordToLogs(&feature_triggered_in_session_);

  EXPECT_TRUE(feature_triggered_in_session_.empty());

  histogram_.ExpectUniqueSample(
      "Omnibox.RichAutocompletion.Triggered",
      AutocompleteMatch::RichAutocompletionType::kNone, 1);
  histogram_.ExpectUniqueSample("Omnibox.RichAutocompletion.Triggered.Any",
                                false, 1);

  {
    SCOPED_TRACE("Reset session");
    base::HistogramTester histogram;
    service_.ResetSession();
    service_.RecordToLogs(&feature_triggered_in_session_);
    EXPECT_TRUE(feature_triggered_in_session_.empty());
    histogram.ExpectTotalCount("Omnibox.RichAutocompletion.Triggered", 0);
    histogram.ExpectUniqueSample("Omnibox.RichAutocompletion.Triggered.Any",
                                 false, 1);
  }
}

TEST_F(OmniboxTriggeredFeatureServiceTest, RichAutocompletionTypeTriggered) {
  // Simulate 4 updates in the session, 3 of which had rich
  // autocompletion, of 2 different types.
  service_.FeatureTriggered(
      OmniboxTriggeredFeatureService::Feature::kRichAutocompletion);
  service_.RichAutocompletionTypeTriggered(
      AutocompleteMatch::RichAutocompletionType::kTitleNonPrefix);
  service_.RichAutocompletionTypeTriggered(
      AutocompleteMatch::RichAutocompletionType::kTitlePrefix);
  service_.RichAutocompletionTypeTriggered(
      AutocompleteMatch::RichAutocompletionType::kTitlePrefix);
  service_.RichAutocompletionTypeTriggered(
      AutocompleteMatch::RichAutocompletionType::kNone);

  service_.RecordToLogs(&feature_triggered_in_session_);

  EXPECT_THAT(
      feature_triggered_in_session_,
      testing::ElementsAre(
          OmniboxTriggeredFeatureService::Feature::kRichAutocompletion));

  histogram_.ExpectTotalCount("Omnibox.RichAutocompletion.Triggered", 3);
  histogram_.ExpectBucketCount(
      "Omnibox.RichAutocompletion.Triggered",
      AutocompleteMatch::RichAutocompletionType::kTitleNonPrefix, 1);
  histogram_.ExpectBucketCount(
      "Omnibox.RichAutocompletion.Triggered",
      AutocompleteMatch::RichAutocompletionType::kTitlePrefix, 1);
  histogram_.ExpectBucketCount("Omnibox.RichAutocompletion.Triggered",
                               AutocompleteMatch::RichAutocompletionType::kNone,
                               1);
  histogram_.ExpectUniqueSample("Omnibox.RichAutocompletion.Triggered.Any",
                                true, 1);

  {
    SCOPED_TRACE("Reset session");
    base::HistogramTester histogram;
    service_.ResetSession();
    service_.RecordToLogs(&feature_triggered_in_session_);
    EXPECT_TRUE(feature_triggered_in_session_.empty());
    histogram.ExpectTotalCount("Omnibox.RichAutocompletion.Triggered", 0);
    histogram.ExpectUniqueSample("Omnibox.RichAutocompletion.Triggered.Any",
                                 false, 1);
  }
}
