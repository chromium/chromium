// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/interaction_to_next_paint_calculator.h"

#include "base/test/scoped_feature_list.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

using EventTiming = page_load_metrics::mojom::EventTiming;

class InteractionToNextPaintCalculatorTest
    : public content::RenderViewHostTestHarness {
 public:
  InteractionToNextPaintCalculatorTest() = default;

  void AddNewEventTimings(
      const content::RenderFrameHost* source,
      std::vector<page_load_metrics::mojom::EventTimingPtr>& event_timings) {
    interaction_to_next_paint_calculator_.AddNewEventTimings(*source,
                                                             event_timings);
  }

  uint64_t GetNumInteractions() {
    return interaction_to_next_paint_calculator_.num_user_interactions();
  }

  EventTiming GetWorstInteraction() {
    return interaction_to_next_paint_calculator_.worst_latency()->max_event;
  }

  EventTiming GetHighPercentileInteraction() {
    return interaction_to_next_paint_calculator_.ApproximateHighPercentile()
        ->max_event;
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
      base::Milliseconds(3000), 1, current_time + base::Milliseconds(1000)));
  event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(3500), 2, current_time + base::Milliseconds(2000)));
  event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(2000), 3, current_time + base::Milliseconds(3000)));
  AddNewEventTimings(main_rfh(), event_timings);
  EXPECT_EQ(GetNumInteractions(), 3u);
  EXPECT_EQ(GetWorstInteraction().duration, base::Milliseconds(3500));
  EXPECT_EQ(GetWorstInteraction().interaction_id, 2u);
  EXPECT_EQ(GetWorstInteraction().start_time,
            current_time + base::Milliseconds(2000));
  EXPECT_EQ(GetHighPercentileInteraction().duration, base::Milliseconds(3500));
  EXPECT_EQ(GetHighPercentileInteraction().interaction_id, 2u);
  EXPECT_EQ(GetHighPercentileInteraction().start_time,
            current_time + base::Milliseconds(2000));

  // After adding 50 additional interactions, the high percentile should shift
  // to the second highest interaction duration.
  event_timings.clear();
  for (uint64_t i = 1; i <= 50; i++) {
    event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
        base::Milliseconds(i + 100), i + 3,
        current_time + base::Milliseconds(4000 + (1000 * (i - 1)))));
  }
  AddNewEventTimings(main_rfh(), event_timings);
  EXPECT_EQ(GetNumInteractions(), 53u);
  EXPECT_EQ(GetWorstInteraction().duration, base::Milliseconds(3500));
  EXPECT_EQ(GetWorstInteraction().interaction_id, 2u);
  EXPECT_EQ(GetWorstInteraction().start_time,
            current_time + base::Milliseconds(2000));
  EXPECT_EQ(GetHighPercentileInteraction().duration, base::Milliseconds(3000));
  EXPECT_EQ(GetHighPercentileInteraction().interaction_id, 1u);
  EXPECT_EQ(GetHighPercentileInteraction().start_time,
            current_time + base::Milliseconds(1000));

  // After adding 50 more interactions, the high percentile should shift
  // to the third highest interaction duration.
  event_timings.clear();
  for (uint64_t i = 1; i <= 50; i++) {
    event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
        base::Milliseconds(300 - i + 1), i + 53,
        current_time + base::Milliseconds(53000 + (1000 * (i - 1)))));
  }
  AddNewEventTimings(main_rfh(), event_timings);
  EXPECT_EQ(GetNumInteractions(), 103u);
  EXPECT_EQ(GetWorstInteraction().duration, base::Milliseconds(3500));
  EXPECT_EQ(GetWorstInteraction().interaction_id, 2u);
  EXPECT_EQ(GetWorstInteraction().start_time,
            current_time + base::Milliseconds(2000));
  EXPECT_EQ(GetHighPercentileInteraction().duration, base::Milliseconds(2000));
  EXPECT_EQ(GetHighPercentileInteraction().interaction_id, 3u);
  EXPECT_EQ(GetHighPercentileInteraction().start_time,
            current_time + base::Milliseconds(3000));
}

TEST_F(InteractionToNextPaintCalculatorTest, TooManyInteractions) {
  // Test what happens when there are so many interactions that the INP index
  // goes out of the event_timings_ bounds.
  base::TimeTicks current_time = base::TimeTicks::Now();
  for (uint64_t i = 1; i <= 500; i++) {
    std::vector<page_load_metrics::mojom::EventTimingPtr> event_timings;
    event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
        base::Milliseconds(3000 - i + 1), i * 2 - 1,
        current_time + base::Milliseconds(1000)));
    event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
        base::Milliseconds(2000 - i + 1), i * 2,
        current_time + base::Milliseconds(1100)));
    AddNewEventTimings(main_rfh(), event_timings);
  }
  EXPECT_EQ(GetNumInteractions(), 1000u);
  EXPECT_EQ(GetWorstInteraction().duration, base::Milliseconds(3000));
  EXPECT_EQ(GetHighPercentileInteraction().duration, base::Milliseconds(2991));
}

TEST_F(InteractionToNextPaintCalculatorTest, MultipleEventsPerInteraction) {
  std::vector<page_load_metrics::mojom::EventTimingPtr> event_timings;
  base::TimeTicks current_time = base::TimeTicks::Now();

  // Interaction 1 with two events.
  event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(100), 1, current_time + base::Milliseconds(100)));
  event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(300), 1, current_time + base::Milliseconds(200)));
  AddNewEventTimings(main_rfh(), event_timings);

  EXPECT_EQ(GetNumInteractions(), 1u);
  EXPECT_EQ(GetWorstInteraction().duration, base::Milliseconds(300));

  // Interaction 2 with one event.
  event_timings.clear();
  event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(200), 2, current_time + base::Milliseconds(300)));
  AddNewEventTimings(main_rfh(), event_timings);

  EXPECT_EQ(GetNumInteractions(), 2u);
  EXPECT_EQ(GetWorstInteraction().duration, base::Milliseconds(300));

  // Interaction 2 with a new, longer event.
  event_timings.clear();
  event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(400), 2, current_time + base::Milliseconds(400)));
  AddNewEventTimings(main_rfh(), event_timings);

  EXPECT_EQ(GetNumInteractions(), 2u);
  EXPECT_EQ(GetWorstInteraction().duration, base::Milliseconds(400));
}

TEST_F(InteractionToNextPaintCalculatorTest, MultipleSources) {
  std::vector<page_load_metrics::mojom::EventTimingPtr> event_timings;
  base::TimeTicks current_time = base::TimeTicks::Now();

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  rfh_tester->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  content::RenderFrameHostTester::For(subframe)
      ->InitializeRenderFrameIfNeeded();

  // Source 1 (main frame), interaction 1.
  event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(100), 1, current_time + base::Milliseconds(100)));
  AddNewEventTimings(main_rfh(), event_timings);

  // Source 2 (subframe), interaction 1 (should be a new interaction).
  event_timings.clear();
  event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(200), 1, current_time + base::Milliseconds(200)));
  AddNewEventTimings(subframe, event_timings);

  EXPECT_EQ(GetNumInteractions(), 2u);
  EXPECT_EQ(GetWorstInteraction().duration, base::Milliseconds(200));
}

TEST_F(InteractionToNextPaintCalculatorTest, MultipleFramesWithConsecutiveIds) {
  std::vector<page_load_metrics::mojom::EventTimingPtr> event_timings;
  base::TimeTicks current_time = base::TimeTicks::Now();

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  rfh_tester->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  content::RenderFrameHostTester::For(subframe)
      ->InitializeRenderFrameIfNeeded();

  // Main frame: IDs 1, 2, 3.
  event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(100), 1, current_time + base::Milliseconds(100)));
  event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(100), 2, current_time + base::Milliseconds(200)));
  event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(100), 3, current_time + base::Milliseconds(300)));
  AddNewEventTimings(main_rfh(), event_timings);

  // Subframe: IDs 11, 12.
  event_timings.clear();
  event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(100), 11, current_time + base::Milliseconds(400)));
  event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(100), 12, current_time + base::Milliseconds(500)));
  AddNewEventTimings(subframe, event_timings);

  // Total interactions should be (3-1+1) + (12-11+1) = 3 + 2 = 5.
  EXPECT_EQ(GetNumInteractions(), 5u);
}

TEST_F(InteractionToNextPaintCalculatorTest, GapsInInteractionIds) {
  std::vector<page_load_metrics::mojom::EventTimingPtr> event_timings;
  base::TimeTicks current_time = base::TimeTicks::Now();

  // Main frame: 1, 10
  event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(100), 1, current_time + base::Milliseconds(100)));
  event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(100), 10, current_time + base::Milliseconds(1000)));
  AddNewEventTimings(main_rfh(), event_timings);

  // Heuristic assumes IDs 1..10 were all present.
  EXPECT_EQ(GetNumInteractions(), 10u);
}

TEST_F(InteractionToNextPaintCalculatorTest, SoftNavigationInteractionIds) {
  std::vector<page_load_metrics::mojom::EventTimingPtr> event_timings;
  base::TimeTicks current_time = base::TimeTicks::Now();

  // Soft navigation might report interactions starting from a large ID.
  event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(100), 101, current_time + base::Milliseconds(100)));
  event_timings.emplace_back(page_load_metrics::mojom::EventTiming::New(
      base::Milliseconds(100), 105, current_time + base::Milliseconds(500)));
  AddNewEventTimings(main_rfh(), event_timings);

  // 105 - 101 + 1 = 5.
  EXPECT_EQ(GetNumInteractions(), 5u);
}
