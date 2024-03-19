// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_triggered_feature_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

class OmniboxTriggeredFeatureServiceTest : public testing::Test {
 public:
  OmniboxTriggeredFeatureServiceTest() = default;

  ~OmniboxTriggeredFeatureServiceTest() override = default;

  void RecordAndExpectFeatures(
      OmniboxTriggeredFeatureService::Features features) {
    RecordAndExpectFeatures(features, features);
  }

  void RecordAndExpectFeatures(
      OmniboxTriggeredFeatureService::Features features,
      OmniboxTriggeredFeatureService::Features features_triggered_in_session) {
    service_.RecordToLogs(&feature_triggered_, &feature_triggered_in_session_);
    EXPECT_THAT(feature_triggered_, testing::ElementsAreArray(features));
    EXPECT_THAT(feature_triggered_in_session_,
                testing::ElementsAreArray(features_triggered_in_session));
  }

  OmniboxTriggeredFeatureService service_;
  OmniboxTriggeredFeatureService::Features feature_triggered_;
  OmniboxTriggeredFeatureService::Features feature_triggered_in_session_;
  base::HistogramTester histogram_;
};

TEST_F(OmniboxTriggeredFeatureServiceTest, NoFeaturesTriggered) {
  RecordAndExpectFeatures({});

  histogram_.ExpectTotalCount("Omnibox.RichAutocompletion.Triggered", 0);
  histogram_.ExpectUniqueSample("Omnibox.RichAutocompletion.Triggered.Any",
                                false, 1);
}

TEST_F(OmniboxTriggeredFeatureServiceTest, TwoFeaturesTriggered) {
  service_.FeatureTriggered(
      metrics::OmniboxEventProto_Feature_REMOTE_SEARCH_FEATURE);
  service_.FeatureTriggered(
      metrics::
          OmniboxEventProto_Feature_SHORT_BOOKMARK_SUGGESTIONS_BY_TOTAL_INPUT_LENGTH);
  RecordAndExpectFeatures(
      {metrics::OmniboxEventProto_Feature_REMOTE_SEARCH_FEATURE,
       metrics::
           OmniboxEventProto_Feature_SHORT_BOOKMARK_SUGGESTIONS_BY_TOTAL_INPUT_LENGTH});

  histogram_.ExpectTotalCount("Omnibox.RichAutocompletion.Triggered", 0);
  histogram_.ExpectUniqueSample("Omnibox.RichAutocompletion.Triggered.Any",
                                false, 1);

  service_.ResetSession();
  RecordAndExpectFeatures({});
}

TEST_F(OmniboxTriggeredFeatureServiceTest, TriggerRichAutocompletionType_kNo) {
  // Simulate 2 updates in the session, neither of which had rich
  // autocompletion.
  service_.RichAutocompletionTypeTriggered(
      AutocompleteMatch::RichAutocompletionType::kNone);
  service_.RichAutocompletionTypeTriggered(
      AutocompleteMatch::RichAutocompletionType::kNone);

  RecordAndExpectFeatures({});

  histogram_.ExpectUniqueSample(
      "Omnibox.RichAutocompletion.Triggered",
      AutocompleteMatch::RichAutocompletionType::kNone, 1);
  histogram_.ExpectUniqueSample("Omnibox.RichAutocompletion.Triggered.Any",
                                false, 1);

  {
    SCOPED_TRACE("Reset session");
    base::HistogramTester histogram;
    service_.ResetSession();
    RecordAndExpectFeatures({});
    histogram.ExpectTotalCount("Omnibox.RichAutocompletion.Triggered", 0);
    histogram.ExpectUniqueSample("Omnibox.RichAutocompletion.Triggered.Any",
                                 false, 1);
  }
}

TEST_F(OmniboxTriggeredFeatureServiceTest, RichAutocompletionTypeTriggered) {
  // Simulate 4 updates in the session, 3 of which had rich
  // autocompletion, of 2 different types.
  service_.FeatureTriggered(
      metrics::OmniboxEventProto_Feature_RICH_AUTOCOMPLETION);
  service_.RichAutocompletionTypeTriggered(
      AutocompleteMatch::RichAutocompletionType::kTitleNonPrefix);
  service_.RichAutocompletionTypeTriggered(
      AutocompleteMatch::RichAutocompletionType::kTitlePrefix);
  service_.RichAutocompletionTypeTriggered(
      AutocompleteMatch::RichAutocompletionType::kTitlePrefix);
  service_.RichAutocompletionTypeTriggered(
      AutocompleteMatch::RichAutocompletionType::kNone);

  RecordAndExpectFeatures(
      {metrics::OmniboxEventProto_Feature_RICH_AUTOCOMPLETION});

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
    RecordAndExpectFeatures({});
    histogram.ExpectTotalCount("Omnibox.RichAutocompletion.Triggered", 0);
    histogram.ExpectUniqueSample("Omnibox.RichAutocompletion.Triggered.Any",
                                 false, 1);
  }
}

TEST_F(OmniboxTriggeredFeatureServiceTest, ResetInput) {
  service_.FeatureTriggered(
      metrics::OmniboxEventProto_Feature_REMOTE_SEARCH_FEATURE);
  service_.ResetInput();
  service_.FeatureTriggered(
      metrics::
          OmniboxEventProto_Feature_SHORT_BOOKMARK_SUGGESTIONS_BY_TOTAL_INPUT_LENGTH);
  RecordAndExpectFeatures(
      {metrics::
           OmniboxEventProto_Feature_SHORT_BOOKMARK_SUGGESTIONS_BY_TOTAL_INPUT_LENGTH},
      {metrics::OmniboxEventProto_Feature_REMOTE_SEARCH_FEATURE,
       metrics::
           OmniboxEventProto_Feature_SHORT_BOOKMARK_SUGGESTIONS_BY_TOTAL_INPUT_LENGTH});
}
