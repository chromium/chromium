// Copyright 2021 The Chromium Authors
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
                              UserInteractionLatencies& max_event_durations) {
    responsiveness_metrics_normalization_.AddNewUserInteractionLatencies(
        num_new_interactions, max_event_durations);
  }

  uint64_t GetNumInteractions() {
    return responsiveness_metrics_normalization_.num_user_interactions();
  }

  base::TimeDelta GetWorstInteraction() {
    return responsiveness_metrics_normalization_.worst_latency().value();
  }

  base::TimeDelta GetHighPercentileInteraction() {
    return responsiveness_metrics_normalization_.ApproximateHighPercentile()
        .value();
  }

 private:
  page_load_metrics::ResponsivenessMetricsNormalization
      responsiveness_metrics_normalization_;
};

TEST_F(ResponsivenessMetricsNormalizationTest, SendAllInteractions) {
  // Check that we get the correct count, worst, and high percentile
  // with 3 interactions.
  UserInteractionLatenciesPtr user_interaction_latencies_ptr =
      UserInteractionLatencies::NewUserInteractionLatencies({});
  auto& user_interaction_latencies =
      user_interaction_latencies_ptr->get_user_interaction_latencies();
  user_interaction_latencies.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(3000), UserInteractionType::kTapOrClick));
  user_interaction_latencies.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(3500), UserInteractionType::kTapOrClick));
  user_interaction_latencies.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(2000), UserInteractionType::kTapOrClick));
  AddNewUserInteractions(3, *user_interaction_latencies_ptr);
  EXPECT_EQ(GetNumInteractions(), 3u);
  EXPECT_EQ(GetWorstInteraction(), base::Milliseconds(3500));
  EXPECT_EQ(GetHighPercentileInteraction(), base::Milliseconds(3500));

  // After adding 50 additional interactions, the high percentile should shift
  // to the second highest interaction duration.
  user_interaction_latencies.clear();
  for (uint64_t i = 0; i < 50; i++) {
    user_interaction_latencies.emplace_back(UserInteractionLatency::New(
        base::Milliseconds(i + 100), UserInteractionType::kTapOrClick));
  }
  AddNewUserInteractions(50, *user_interaction_latencies_ptr);
  EXPECT_EQ(GetNumInteractions(), 53u);
  EXPECT_EQ(GetWorstInteraction(), base::Milliseconds(3500));
  EXPECT_EQ(GetHighPercentileInteraction(), base::Milliseconds(3000));

  // After adding 50 more interactions, the high percentile should shift
  // to the third highest interaction duration.
  user_interaction_latencies.clear();
  for (uint64_t i = 0; i < 50; i++) {
    user_interaction_latencies.emplace_back(UserInteractionLatency::New(
        base::Milliseconds(300 - i), UserInteractionType::kTapOrClick));
  }
  AddNewUserInteractions(50, *user_interaction_latencies_ptr);
  EXPECT_EQ(GetNumInteractions(), 103u);
  EXPECT_EQ(GetWorstInteraction(), base::Milliseconds(3500));
  EXPECT_EQ(GetHighPercentileInteraction(), base::Milliseconds(2000));
}