// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_triggered_feature_service.h"

#include "testing/gmock/include/gmock/gmock.h"
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
};

TEST_F(OmniboxTriggeredFeatureServiceTest, NoFeaturesTriggered) {
  RecordAndExpectFeatures({});
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

  service_.ResetSession();
  RecordAndExpectFeatures({});
}

TEST_F(OmniboxTriggeredFeatureServiceTest, RichAutocompletionFeatureTriggered) {
  service_.FeatureTriggered(
      metrics::OmniboxEventProto_Feature_RICH_AUTOCOMPLETION);

  RecordAndExpectFeatures(
      {metrics::OmniboxEventProto_Feature_RICH_AUTOCOMPLETION});

  {
    SCOPED_TRACE("Reset session");
    service_.ResetSession();
    RecordAndExpectFeatures({});
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
