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

  UserInteractionLatency GetWorstInteraction() {
    return responsiveness_metrics_normalization_.worst_latency().value();
  }

  UserInteractionLatency GetHighPercentileInteraction() {
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
  base::TimeTicks current_time = base::TimeTicks::Now();
  user_interaction_latencies.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(3000), UserInteractionType::kTapOrClick, 0,
      current_time + base::Milliseconds(1000)));
  user_interaction_latencies.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(3500), UserInteractionType::kTapOrClick, 1,
      current_time + base::Milliseconds(2000)));
  user_interaction_latencies.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(2000), UserInteractionType::kTapOrClick, 2,
      current_time + base::Milliseconds(3000)));
  AddNewUserInteractions(3, *user_interaction_latencies_ptr);
  EXPECT_EQ(GetNumInteractions(), 3u);
  EXPECT_EQ(GetWorstInteraction().interaction_latency,
            base::Milliseconds(3500));
  EXPECT_EQ(GetWorstInteraction().interaction_offset, 1u);
  EXPECT_EQ(GetWorstInteraction().interaction_time,
            current_time + base::Milliseconds(2000));
  EXPECT_EQ(GetHighPercentileInteraction().interaction_latency,
            base::Milliseconds(3500));
  EXPECT_EQ(GetHighPercentileInteraction().interaction_offset, 1u);
  EXPECT_EQ(GetHighPercentileInteraction().interaction_time,
            current_time + base::Milliseconds(2000));

  // After adding 50 additional interactions, the high percentile should shift
  // to the second highest interaction duration.
  user_interaction_latencies.clear();
  for (uint64_t i = 0; i < 50; i++) {
    user_interaction_latencies.emplace_back(UserInteractionLatency::New(
        base::Milliseconds(i + 100), UserInteractionType::kTapOrClick, i + 3,
        current_time + base::Milliseconds(4000 + (1000 * i))));
  }
  AddNewUserInteractions(50, *user_interaction_latencies_ptr);
  EXPECT_EQ(GetNumInteractions(), 53u);
  EXPECT_EQ(GetWorstInteraction().interaction_latency,
            base::Milliseconds(3500));
  EXPECT_EQ(GetWorstInteraction().interaction_offset, 1u);
  EXPECT_EQ(GetWorstInteraction().interaction_time,
            current_time + base::Milliseconds(2000));
  EXPECT_EQ(GetHighPercentileInteraction().interaction_latency,
            base::Milliseconds(3000));
  EXPECT_EQ(GetHighPercentileInteraction().interaction_offset, 0u);
  EXPECT_EQ(GetHighPercentileInteraction().interaction_time,
            current_time + base::Milliseconds(1000));

  // After adding 50 more interactions, the high percentile should shift
  // to the third highest interaction duration.
  user_interaction_latencies.clear();
  for (uint64_t i = 0; i < 50; i++) {
    user_interaction_latencies.emplace_back(UserInteractionLatency::New(
        base::Milliseconds(300 - i), UserInteractionType::kTapOrClick, i + 53,
        current_time + base::Milliseconds(53000 + (1000 * i))));
  }
  AddNewUserInteractions(50, *user_interaction_latencies_ptr);
  EXPECT_EQ(GetNumInteractions(), 103u);
  EXPECT_EQ(GetWorstInteraction().interaction_latency,
            base::Milliseconds(3500));
  EXPECT_EQ(GetWorstInteraction().interaction_offset, 1u);
  EXPECT_EQ(GetWorstInteraction().interaction_time,
            current_time + base::Milliseconds(2000));
  EXPECT_EQ(GetHighPercentileInteraction().interaction_latency,
            base::Milliseconds(2000));
  EXPECT_EQ(GetHighPercentileInteraction().interaction_offset, 2u);
  EXPECT_EQ(GetHighPercentileInteraction().interaction_time,
            current_time + base::Milliseconds(3000));
}

TEST_F(ResponsivenessMetricsNormalizationTest, TooManyInteractions) {
  // Test what happens when there are so many interactions that the INP index
  // goes out of the worst_ten_latencies_ bounds.
  base::TimeTicks current_time = base::TimeTicks::Now();
  for (uint64_t i = 0; i < 500; i++) {
    UserInteractionLatenciesPtr user_interaction_latencies_ptr =
        UserInteractionLatencies::NewUserInteractionLatencies({});
    auto& user_interaction_latencies =
        user_interaction_latencies_ptr->get_user_interaction_latencies();
    user_interaction_latencies.emplace_back(UserInteractionLatency::New(
        base::Milliseconds(3000 - i), UserInteractionType::kTapOrClick, i * 2,
        current_time + base::Milliseconds(1000)));
    user_interaction_latencies.emplace_back(UserInteractionLatency::New(
        base::Milliseconds(2000 - (i)), UserInteractionType::kTapOrClick,
        (i * 2) + i, current_time + base::Milliseconds(1100)));
    AddNewUserInteractions(2, *user_interaction_latencies_ptr);
  }
  EXPECT_EQ(GetNumInteractions(), 1000u);
  EXPECT_EQ(GetWorstInteraction().interaction_latency,
            base::Milliseconds(3000));
  EXPECT_EQ(GetHighPercentileInteraction().interaction_latency,
            base::Milliseconds(2991));
}
