// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/interaction_to_next_paint_calculator.h"

#include "base/test/scoped_feature_list.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

using EventTiming = page_load_metrics::mojom::EventTiming;

class InteractionToNextPaintCalculatorTest : public testing::Test {
 public:
  InteractionToNextPaintCalculatorTest() = default;

  void AddNewEventTimings(
      std::vector<page_load_metrics::mojom::EventTimingPtr>& event_timings) {
    interaction_to_next_paint_calculator_.AddNewEventTimings(event_timings);
  }

  uint64_t GetNumInteractions() {
    return interaction_to_next_paint_calculator_.num_user_interactions();
  }

  EventTiming GetWorstInteraction() {
    return *interaction_to_next_paint_calculator_.worst_latency();
  }

  EventTiming GetHighPercentileInteraction() {
    return *interaction_to_next_paint_calculator_.ApproximateHighPercentile();
  }

 private:
  page_load_metrics::InteractionToNextPaintCalculator
      interaction_to_next_paint_calculator_;
};

TEST_F(InteractionToNextPaintCalculatorTest, SendAllInteractions) {
  // Check that we get the correct count, worst, and high percentile
  // with 3 interactions.
  std::vector<page_load_metrics::mojom::EventTimingPtr> event_timings;
  base::TimeTicks current_time = base::TimeTicks::Now();
  event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(3000), 0, current_time + base::Milliseconds(1000)));
  event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(3500), 1, current_time + base::Milliseconds(2000)));
  event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(2000), 2, current_time + base::Milliseconds(3000)));
  AddNewEventTimings(event_timings);
  EXPECT_EQ(GetNumInteractions(), 3u);
  EXPECT_EQ(GetWorstInteraction().duration, base::Milliseconds(3500));
  EXPECT_EQ(GetWorstInteraction().interaction_id, 1u);
  EXPECT_EQ(GetWorstInteraction().start_time,
            current_time + base::Milliseconds(2000));
  EXPECT_EQ(GetHighPercentileInteraction().duration, base::Milliseconds(3500));
  EXPECT_EQ(GetHighPercentileInteraction().interaction_id, 1u);
  EXPECT_EQ(GetHighPercentileInteraction().start_time,
            current_time + base::Milliseconds(2000));

  // After adding 50 additional interactions, the high percentile should shift
  // to the second highest interaction duration.
  event_timings.clear();
  for (uint64_t i = 0; i < 50; i++) {
    event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
        base::Milliseconds(i + 100), i + 3,
        current_time + base::Milliseconds(4000 + (1000 * i))));
  }
  AddNewEventTimings(event_timings);
  EXPECT_EQ(GetNumInteractions(), 53u);
  EXPECT_EQ(GetWorstInteraction().duration, base::Milliseconds(3500));
  EXPECT_EQ(GetWorstInteraction().interaction_id, 1u);
  EXPECT_EQ(GetWorstInteraction().start_time,
            current_time + base::Milliseconds(2000));
  EXPECT_EQ(GetHighPercentileInteraction().duration, base::Milliseconds(3000));
  EXPECT_EQ(GetHighPercentileInteraction().interaction_id, 0u);
  EXPECT_EQ(GetHighPercentileInteraction().start_time,
            current_time + base::Milliseconds(1000));

  // After adding 50 more interactions, the high percentile should shift
  // to the third highest interaction duration.
  event_timings.clear();
  for (uint64_t i = 0; i < 50; i++) {
    event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
        base::Milliseconds(300 - i), i + 53,
        current_time + base::Milliseconds(53000 + (1000 * i))));
  }
  AddNewEventTimings(event_timings);
  EXPECT_EQ(GetNumInteractions(), 103u);
  EXPECT_EQ(GetWorstInteraction().duration, base::Milliseconds(3500));
  EXPECT_EQ(GetWorstInteraction().interaction_id, 1u);
  EXPECT_EQ(GetWorstInteraction().start_time,
            current_time + base::Milliseconds(2000));
  EXPECT_EQ(GetHighPercentileInteraction().duration, base::Milliseconds(2000));
  EXPECT_EQ(GetHighPercentileInteraction().interaction_id, 2u);
  EXPECT_EQ(GetHighPercentileInteraction().start_time,
            current_time + base::Milliseconds(3000));
}

TEST_F(InteractionToNextPaintCalculatorTest, TooManyInteractions) {
  // Test what happens when there are so many interactions that the INP index
  // goes out of the event_timings_ bounds.
  base::TimeTicks current_time = base::TimeTicks::Now();
  for (uint64_t i = 0; i < 500; i++) {
    std::vector<page_load_metrics::mojom::EventTimingPtr> event_timings;
    event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
        base::Milliseconds(3000 - i), i * 2,
        current_time + base::Milliseconds(1000)));
    event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
        base::Milliseconds(2000 - (i)), (i * 2) + i,
        current_time + base::Milliseconds(1100)));
    AddNewEventTimings(event_timings);
  }
  EXPECT_EQ(GetNumInteractions(), 1000u);
  EXPECT_EQ(GetWorstInteraction().duration, base::Milliseconds(3000));
  EXPECT_EQ(GetHighPercentileInteraction().duration, base::Milliseconds(2991));
}

TEST_F(InteractionToNextPaintCalculatorTest, MergeAndClear) {
  page_load_metrics::InteractionToNextPaintCalculator calculator1;
  page_load_metrics::InteractionToNextPaintCalculator calculator2;

  std::vector<page_load_metrics::mojom::EventTimingPtr> event_timings1;
  base::TimeTicks current_time = base::TimeTicks::Now();
  event_timings1.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(100), 0, current_time + base::Milliseconds(100)));
  event_timings1.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(200), 1, current_time + base::Milliseconds(200)));
  calculator1.AddNewEventTimings(event_timings1);

  std::vector<page_load_metrics::mojom::EventTimingPtr> event_timings2;
  event_timings2.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(300), 2, current_time + base::Milliseconds(300)));
  event_timings2.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(400), 3, current_time + base::Milliseconds(400)));
  calculator2.AddNewEventTimings(event_timings2);

  calculator1.MergeAndClear(&calculator2);

  EXPECT_EQ(calculator1.num_user_interactions(), 4u);
  EXPECT_EQ(calculator1.worst_latency()->duration, base::Milliseconds(400));
  EXPECT_EQ(calculator2.num_user_interactions(), 0u);
  EXPECT_FALSE(calculator2.worst_latency().has_value());
}
