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

TEST_F(ResponsivenessMetricsNormalizationTest, SendAllInteractions) {
  // Flip the flag to send all user interaction latencies to browser.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      blink::features::kSendAllUserInteractionLatencies);

  UserInteractionLatenciesPtr max_event_durations =
      UserInteractionLatencies::NewUserInteractionLatencies({});
  auto& user_interaction_latencies1 =
      max_event_durations->get_user_interaction_latencies();
  user_interaction_latencies1.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(50), UserInteractionType::kKeyboard));
  user_interaction_latencies1.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(100), UserInteractionType::kTapOrClick));
  user_interaction_latencies1.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(150), UserInteractionType::kDrag));

  UserInteractionLatenciesPtr total_event_durations =
      UserInteractionLatencies::NewUserInteractionLatencies({});
  auto& user_interaction_latencies2 =
      total_event_durations->get_user_interaction_latencies();
  user_interaction_latencies2.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(55), UserInteractionType::kKeyboard));
  user_interaction_latencies2.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(105), UserInteractionType::kTapOrClick));
  user_interaction_latencies2.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(155), UserInteractionType::kDrag));

  AddNewUserInteractions(3, *max_event_durations, *total_event_durations);
  auto worst_ten_max_event_durations =
      normalized_responsiveness_metrics()
          .normalized_max_event_durations.worst_ten_latencies_over_budget;
  EXPECT_EQ(worst_ten_max_event_durations.size(), 3u);
  EXPECT_EQ(worst_ten_max_event_durations.top(), base::Milliseconds(0));
  worst_ten_max_event_durations.pop();
  EXPECT_EQ(worst_ten_max_event_durations.top(), base::Milliseconds(0));
  worst_ten_max_event_durations.pop();
  EXPECT_EQ(worst_ten_max_event_durations.top(), base::Milliseconds(50));

  auto worst_ten_total_event_durations =
      normalized_responsiveness_metrics()
          .normalized_total_event_durations.worst_ten_latencies_over_budget;
  EXPECT_EQ(worst_ten_total_event_durations.size(), 3u);
  EXPECT_EQ(worst_ten_total_event_durations.top(), base::Milliseconds(5));
  worst_ten_total_event_durations.pop();
  EXPECT_EQ(worst_ten_total_event_durations.top(), base::Milliseconds(5));
  worst_ten_total_event_durations.pop();
  EXPECT_EQ(worst_ten_total_event_durations.top(), base::Milliseconds(55));
  EXPECT_EQ(normalized_responsiveness_metrics().num_user_interactions, 3u);

  auto& normalized_max_event_durations =
      normalized_responsiveness_metrics().normalized_max_event_durations;
  EXPECT_EQ(normalized_max_event_durations.worst_latency,
            base::Milliseconds(150));
  EXPECT_EQ(normalized_max_event_durations.worst_latency_over_budget,
            base::Milliseconds(50));
  EXPECT_EQ(normalized_max_event_durations.sum_of_latency_over_budget,
            base::Milliseconds(50));
  EXPECT_EQ(
      normalized_max_event_durations.pseudo_second_worst_latency_over_budget,
      base::Milliseconds(0));

  auto& normalized_total_event_durations =
      normalized_responsiveness_metrics().normalized_total_event_durations;
  EXPECT_EQ(normalized_total_event_durations.worst_latency,
            base::Milliseconds(155));
  EXPECT_EQ(normalized_total_event_durations.worst_latency_over_budget,
            base::Milliseconds(55));
  EXPECT_EQ(normalized_total_event_durations.sum_of_latency_over_budget,
            base::Milliseconds(65));
  EXPECT_EQ(
      normalized_total_event_durations.pseudo_second_worst_latency_over_budget,
      base::Milliseconds(5));
}