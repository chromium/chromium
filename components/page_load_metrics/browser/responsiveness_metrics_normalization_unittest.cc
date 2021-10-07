// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/responsiveness_metrics_normalization.h"
#include "base/test/scoped_feature_list.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

using UserInteractionLatenciesPtr =
    page_load_metrics::mojom::UserInteractionLatenciesPtr;
using UserInteractionLatencies =
    page_load_metrics::mojom::UserInteractionLatencies;
using UserInteractionLatency = page_load_metrics::mojom::UserInteractionLatency;
using UserInteractionType = page_load_metrics::mojom::UserInteractionType;

class ResponsivenessMetricsNormalizationTest : public testing::Test {
 public:
  ResponsivenessMetricsNormalizationTest() = default;

  void AddNewUserInteractions(uint64_t num_new_interactions,
                              UserInteractionLatencies& max_event_durations,
                              UserInteractionLatencies& total_event_durations) {
    responsiveness_metrics_normalization_.AddNewUserInteractionLatencies(
        num_new_interactions, max_event_durations, total_event_durations);
  }

  const page_load_metrics::NormalizedResponsivenessMetrics&
  normalized_responsiveness_metrics() const {
    return responsiveness_metrics_normalization_
        .GetNormalizedResponsivenessMetrics();
  }

 private:
  page_load_metrics::ResponsivenessMetricsNormalization
      responsiveness_metrics_normalization_;
};

TEST_F(ResponsivenessMetricsNormalizationTest, OnlySendWorstInteractions) {
  UserInteractionLatenciesPtr max_event_duration1 =
      UserInteractionLatencies::NewWorstInteractionLatency(
          base::TimeDelta::FromMilliseconds(120));
  UserInteractionLatenciesPtr total_event_durations1 =
      UserInteractionLatencies::NewWorstInteractionLatency(
          base::TimeDelta::FromMilliseconds(140));
  AddNewUserInteractions(1, *max_event_duration1, *total_event_durations1);

  UserInteractionLatenciesPtr max_event_duration2 =
      UserInteractionLatencies::NewWorstInteractionLatency(
          base::TimeDelta::FromMilliseconds(200));
  UserInteractionLatenciesPtr total_event_durations2 =
      UserInteractionLatencies::NewWorstInteractionLatency(
          base::TimeDelta::FromMilliseconds(250));
  AddNewUserInteractions(2, *max_event_duration2, *total_event_durations2);

  UserInteractionLatenciesPtr max_event_duration3 =
      UserInteractionLatencies::NewWorstInteractionLatency(
          base::TimeDelta::FromMilliseconds(70));
  UserInteractionLatenciesPtr total_event_durations3 =
      UserInteractionLatencies::NewWorstInteractionLatency(
          base::TimeDelta::FromMilliseconds(70));
  AddNewUserInteractions(3, *max_event_duration3, *total_event_durations3);

  EXPECT_EQ(normalized_responsiveness_metrics().num_user_interactions, 6u);
  EXPECT_EQ(normalized_responsiveness_metrics()
                .normalized_max_event_durations.worst_latency,
            base::TimeDelta::FromMilliseconds(200));
  // When the flag is disabled, only worst_latency has a meaningful value and
  // other metrics should have default values.
  EXPECT_EQ(normalized_responsiveness_metrics()
                .normalized_max_event_durations.worst_latency_over_budget,
            base::TimeDelta::FromMilliseconds(0));
  EXPECT_EQ(normalized_responsiveness_metrics()
                .normalized_max_event_durations.total_latency_over_budget,
            base::TimeDelta::FromMilliseconds(0));
  EXPECT_EQ(normalized_responsiveness_metrics()
                .normalized_max_event_durations
                .pseudo_second_worst_latency_over_budget,
            base::TimeDelta::FromMilliseconds(0));
  EXPECT_EQ(normalized_responsiveness_metrics()
                .normalized_total_event_durations.worst_latency,
            base::TimeDelta::FromMilliseconds(250));
  EXPECT_EQ(normalized_responsiveness_metrics()
                .normalized_total_event_durations.worst_latency_over_budget,
            base::TimeDelta::FromMilliseconds(0));
  EXPECT_EQ(normalized_responsiveness_metrics()
                .normalized_total_event_durations.total_latency_over_budget,
            base::TimeDelta::FromMilliseconds(0));
  EXPECT_EQ(normalized_responsiveness_metrics()
                .normalized_total_event_durations
                .pseudo_second_worst_latency_over_budget,
            base::TimeDelta::FromMilliseconds(0));
}

TEST_F(ResponsivenessMetricsNormalizationTest, SendAllInteractions) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      blink::features::kSendAllUserInteractionLatencies);
  UserInteractionLatenciesPtr max_event_duration =
      UserInteractionLatencies::NewUserInteractionLatencies({});
  max_event_duration->get_user_interaction_latencies().emplace_back(
      UserInteractionLatency::New(base::TimeDelta::FromMilliseconds(50),
                                  UserInteractionType::kKeyboard));
  max_event_duration->get_user_interaction_latencies().emplace_back(
      UserInteractionLatency::New(base::TimeDelta::FromMilliseconds(100),
                                  UserInteractionType::kTapOrClick));
  max_event_duration->get_user_interaction_latencies().emplace_back(
      UserInteractionLatency::New(base::TimeDelta::FromMilliseconds(150),
                                  UserInteractionType::kDrag));

  UserInteractionLatenciesPtr total_event_durations =
      UserInteractionLatencies::NewUserInteractionLatencies({});
  total_event_durations->get_user_interaction_latencies().emplace_back(
      UserInteractionLatency::New(base::TimeDelta::FromMilliseconds(55),
                                  UserInteractionType::kKeyboard));
  total_event_durations->get_user_interaction_latencies().emplace_back(
      UserInteractionLatency::New(base::TimeDelta::FromMilliseconds(105),
                                  UserInteractionType::kTapOrClick));
  total_event_durations->get_user_interaction_latencies().emplace_back(
      UserInteractionLatency::New(base::TimeDelta::FromMilliseconds(155),
                                  UserInteractionType::kDrag));

  AddNewUserInteractions(3, *max_event_duration, *total_event_durations);
  EXPECT_EQ(normalized_responsiveness_metrics().num_user_interactions, 3u);
  EXPECT_EQ(normalized_responsiveness_metrics()
                .normalized_max_event_durations.worst_latency,
            base::TimeDelta::FromMilliseconds(150));
  EXPECT_EQ(normalized_responsiveness_metrics()
                .normalized_max_event_durations.worst_latency_over_budget,
            base::TimeDelta::FromMilliseconds(50));
  EXPECT_EQ(normalized_responsiveness_metrics()
                .normalized_max_event_durations.total_latency_over_budget,
            base::TimeDelta::FromMilliseconds(50));
  EXPECT_EQ(normalized_responsiveness_metrics()
                .normalized_max_event_durations
                .pseudo_second_worst_latency_over_budget,
            base::TimeDelta::FromMilliseconds(0));
  EXPECT_EQ(normalized_responsiveness_metrics()
                .normalized_total_event_durations.worst_latency,
            base::TimeDelta::FromMilliseconds(155));
  EXPECT_EQ(normalized_responsiveness_metrics()
                .normalized_total_event_durations.worst_latency_over_budget,
            base::TimeDelta::FromMilliseconds(55));
  EXPECT_EQ(normalized_responsiveness_metrics()
                .normalized_total_event_durations.total_latency_over_budget,
            base::TimeDelta::FromMilliseconds(65));
  EXPECT_EQ(normalized_responsiveness_metrics()
                .normalized_total_event_durations
                .pseudo_second_worst_latency_over_budget,
            base::TimeDelta::FromMilliseconds(5));
}