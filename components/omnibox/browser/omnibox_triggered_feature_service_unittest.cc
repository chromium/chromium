// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_triggered_feature_service.h"

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
    EXPECT_EQ(feature_triggered_, features);
    EXPECT_EQ(feature_triggered_in_session_, features_triggered_in_session);
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

TEST_F(OmniboxTriggeredFeatureServiceTest, GetFeatureTriggeredInSession) {
  EXPECT_FALSE(service_.GetFeatureTriggeredInSession(
      metrics::OmniboxEventProto_Feature_REMOTE_SEARCH_FEATURE));

  service_.FeatureTriggered(
      metrics::OmniboxEventProto_Feature_REMOTE_SEARCH_FEATURE);
  EXPECT_TRUE(service_.GetFeatureTriggeredInSession(
      metrics::OmniboxEventProto_Feature_REMOTE_SEARCH_FEATURE));
  EXPECT_FALSE(service_.GetFeatureTriggeredInSession(
      metrics::OmniboxEventProto_Feature_ML_URL_SCORING));

  // Starting a new input preserves session-level feature tracking.
  service_.ResetInput();
  EXPECT_TRUE(service_.GetFeatureTriggeredInSession(
      metrics::OmniboxEventProto_Feature_REMOTE_SEARCH_FEATURE));

  // Starting a new session clears session-level feature tracking.
  service_.ResetSession();
  EXPECT_FALSE(service_.GetFeatureTriggeredInSession(
      metrics::OmniboxEventProto_Feature_REMOTE_SEARCH_FEATURE));
}

TEST_F(OmniboxTriggeredFeatureServiceTest, BoundaryFeaturesAndOverwrite) {
  // Pre-populate output sets to verify RecordToLogs overwrites stale bits.
  feature_triggered_ = {
      metrics::OmniboxEventProto_Feature_REMOTE_SEARCH_FEATURE};
  feature_triggered_in_session_ = {
      metrics::OmniboxEventProto_Feature_REMOTE_SEARCH_FEATURE};

  service_.FeatureTriggered(metrics::OmniboxEventProto::Feature_MIN);
  service_.FeatureTriggered(metrics::OmniboxEventProto::Feature_MAX);
  service_.FeatureTriggered(metrics::OmniboxEventProto::Feature_MAX);

  RecordAndExpectFeatures({metrics::OmniboxEventProto::Feature_MIN,
                           metrics::OmniboxEventProto::Feature_MAX});
}
