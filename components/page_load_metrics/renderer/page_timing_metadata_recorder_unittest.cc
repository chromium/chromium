// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/page_timing_metadata_recorder.h"

#include <string_view>
#include <vector>

#include "base/profiler/sample_metadata.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_load_metrics {

struct MetadataTaggingRequest {
  base::TimeTicks period_start;
  base::TimeTicks period_end;
  std::string_view name;
  int64_t key;
  int64_t value;
};

class TestPageTimingMetadataRecorder : public PageTimingMetadataRecorder {
 public:
  explicit TestPageTimingMetadataRecorder(const MonotonicTiming& initial_timing,
                                          bool is_main_frame = false)
      : PageTimingMetadataRecorder(initial_timing, is_main_frame) {}

  void ApplyMetadataToPastSamples(base::TimeTicks period_start,
                                  base::TimeTicks period_end,
                                  std::string_view name,
                                  int64_t key,
                                  int64_t value,
                                  base::SampleMetadataScope scope) override {
    requests_.push_back({
        period_start,
        period_end,
        name,
        key,
        value,
    });
  }

  const std::vector<MetadataTaggingRequest>& GetMetadataTaggingRequests()
      const {
    return requests_;
  }

 private:
  std::vector<MetadataTaggingRequest> requests_;
};

using PageTimingMetadataRecorderTest = testing::Test;

TEST_F(PageTimingMetadataRecorderTest, FirstContentfulPaintUpdate) {
  PageTimingMetadataRecorder::MonotonicTiming timing;
  // The PageTimingMetadataRecorder constructor is supposed to call
  // UpdateMetadata once, but due to class construction limitation, the
  // call to ApplyMetadataToPastSample will not be captured by test class,
  // as the test class is not ready yet.
  TestPageTimingMetadataRecorder recorder(timing);
  const std::vector<MetadataTaggingRequest>& requests =
      recorder.GetMetadataTaggingRequests();

  timing.navigation_start = base::TimeTicks::Now() - base::Milliseconds(500);
  timing.first_contentful_paint =
      *timing.navigation_start + base::Milliseconds(10);
  recorder.UpdateMetadata(timing);
  ASSERT_EQ(1u, requests.size());
  EXPECT_EQ(*timing.navigation_start, requests.at(0).period_start);
  EXPECT_EQ(*timing.first_contentful_paint, requests.at(0).period_end);
  EXPECT_EQ("PageLoad.PaintTiming.NavigationToFirstContentfulPaint",
            requests.at(0).name);

  // Update first contentful paint timetick should sends another request.
  timing.first_contentful_paint =
      *timing.navigation_start + base::Milliseconds(20);
  recorder.UpdateMetadata(timing);
  ASSERT_EQ(2u, requests.size());
  EXPECT_EQ(*timing.navigation_start, requests.at(1).period_start);
  EXPECT_EQ(*timing.first_contentful_paint, requests.at(1).period_end);
  EXPECT_EQ("PageLoad.PaintTiming.NavigationToFirstContentfulPaint",
            requests.at(1).name);

  // If nothing modified, should not send any requests.
  recorder.UpdateMetadata(timing);
  EXPECT_EQ(2u, requests.size());
}

TEST_F(PageTimingMetadataRecorderTest, FirstContentfulPaintInvalidDuration) {
  PageTimingMetadataRecorder::MonotonicTiming timing;
  TestPageTimingMetadataRecorder recorder(timing);
  const std::vector<MetadataTaggingRequest>& requests =
      recorder.GetMetadataTaggingRequests();

  timing.navigation_start = base::TimeTicks::Now() - base::Milliseconds(500);
  // Should reject period_end in the future.
  timing.first_contentful_paint = base::TimeTicks::Now() + base::Hours(1);
  recorder.UpdateMetadata(timing);
  EXPECT_EQ(0u, requests.size());

  // Should reject period_end > period_start.
  timing.first_contentful_paint = *timing.navigation_start - base::Hours(1);
  recorder.UpdateMetadata(timing);
  EXPECT_EQ(0u, requests.size());

  // Should accept period_end == period_start.
  timing.first_contentful_paint = *timing.navigation_start;
  recorder.UpdateMetadata(timing);
  EXPECT_EQ(1u, requests.size());
}

TEST_F(PageTimingMetadataRecorderTest, FirstInputDelayUpdate) {
  PageTimingMetadataRecorder::MonotonicTiming timing;
  // The PageTimingMetadataRecorder constructor is supposed to call
  // UpdateMetadata once, but due to class construction limitation, the
  // call to ApplyMetadataToPastSample will not be captured by test class,
  // as the test class is not ready yet.
  TestPageTimingMetadataRecorder recorder(timing);
  const std::vector<MetadataTaggingRequest>& requests =
      recorder.GetMetadataTaggingRequests();

  timing.first_input_delay = base::Milliseconds(10);
  timing.first_input_timestamp = base::TimeTicks::Now();
  recorder.UpdateMetadata(timing);
  ASSERT_EQ(1u, requests.size());
  EXPECT_EQ(*timing.first_input_timestamp, requests.at(0).period_start);
  EXPECT_EQ(*timing.first_input_timestamp + *timing.first_input_delay,
            requests.at(0).period_end);
  EXPECT_EQ("PageLoad.InteractiveTiming.FirstInputDelay4", requests.at(0).name);

  // Update first input delay should sends another request.
  timing.first_input_delay = base::Milliseconds(11);
  recorder.UpdateMetadata(timing);
  ASSERT_EQ(2u, requests.size());
  EXPECT_EQ(*timing.first_input_timestamp, requests.at(1).period_start);
  EXPECT_EQ(*timing.first_input_timestamp + *timing.first_input_delay,
            requests.at(1).period_end);
  EXPECT_EQ("PageLoad.InteractiveTiming.FirstInputDelay4", requests.at(1).name);

  // If nothing modified, should not send any requests.
  recorder.UpdateMetadata(timing);
  EXPECT_EQ(2u, requests.size());
}

TEST_F(PageTimingMetadataRecorderTest, FirstInputDelayInvalidDuration) {
  PageTimingMetadataRecorder::MonotonicTiming timing;
  TestPageTimingMetadataRecorder recorder(timing);
  const std::vector<MetadataTaggingRequest>& requests =
      recorder.GetMetadataTaggingRequests();

  timing.first_input_timestamp = base::TimeTicks::Now();
  timing.first_input_delay = base::Hours(-1);
  recorder.UpdateMetadata(timing);
  EXPECT_EQ(0u, requests.size());

  // Should accept period_end == period_start.
  timing.first_input_delay = base::Milliseconds(0);
  recorder.UpdateMetadata(timing);
  EXPECT_EQ(1u, requests.size());
}

TEST_F(PageTimingMetadataRecorderTest, MainFrameLargestContentfulPaintUpdate) {
  PageTimingMetadataRecorder::MonotonicTiming timing;
  // The PageTimingMetadataRecorder constructor is supposed to call
  // UpdateMetadata once, but due to class construction limitation, the
  // call to ApplyMetadataToPastSample will not be captured by test class,
  // as the test class is not ready yet.
  TestPageTimingMetadataRecorder recorder(timing, /*is_main_frame=*/true);
  const std::vector<MetadataTaggingRequest>& requests =
      recorder.GetMetadataTaggingRequests();

  timing.navigation_start = base::TimeTicks::Now() - base::Milliseconds(500);
  timing.frame_largest_contentful_paint =
      *timing.navigation_start + base::Milliseconds(10);
  recorder.UpdateMetadata(timing);
  ASSERT_EQ(1u, requests.size());
  EXPECT_EQ(*timing.navigation_start, requests.at(0).period_start);
  EXPECT_EQ(*timing.frame_largest_contentful_paint, requests.at(0).period_end);
  EXPECT_EQ(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.MainFrame",
      requests.at(0).name);
  EXPECT_EQ(10, requests.at(0).value);

  // Update largest contentful paint timedelta should send another request.
  timing.frame_largest_contentful_paint =
      *timing.navigation_start + base::Milliseconds(20);
  recorder.UpdateMetadata(timing);
  ASSERT_EQ(2u, requests.size());
  EXPECT_EQ(*timing.navigation_start, requests.at(1).period_start);
  EXPECT_EQ(*timing.frame_largest_contentful_paint, requests.at(1).period_end);
  EXPECT_EQ(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.MainFrame",
      requests.at(1).name);
  EXPECT_EQ(20, requests.at(1).value);

  // If nothing modified, should not send any requests.
  recorder.UpdateMetadata(timing);
  EXPECT_EQ(2u, requests.size());
}

TEST_F(PageTimingMetadataRecorderTest, SubFrameLargestContentfulPaintUpdate) {
  PageTimingMetadataRecorder::MonotonicTiming timing;
  // The PageTimingMetadataRecorder constructor is supposed to call
  // UpdateMetadata once, but due to class construction limitation, the
  // call to ApplyMetadataToPastSample will not be captured by test class,
  // as the test class is not ready yet.
  TestPageTimingMetadataRecorder recorder(timing, /* is_main_frame */ false);
  const std::vector<MetadataTaggingRequest>& requests =
      recorder.GetMetadataTaggingRequests();

  timing.navigation_start = base::TimeTicks::Now() - base::Milliseconds(500);
  timing.frame_largest_contentful_paint =
      *timing.navigation_start + base::Milliseconds(10);
  recorder.UpdateMetadata(timing);
  ASSERT_EQ(1u, requests.size());
  EXPECT_EQ(*timing.navigation_start, requests.at(0).period_start);
  EXPECT_EQ(*timing.frame_largest_contentful_paint, requests.at(0).period_end);
  EXPECT_EQ("PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.SubFrame",
            requests.at(0).name);
  EXPECT_EQ(10, requests.at(0).value);

  // Update largest contentful paint timedelta should send another request.
  timing.frame_largest_contentful_paint =
      *timing.navigation_start + base::Milliseconds(20);
  recorder.UpdateMetadata(timing);
  ASSERT_EQ(2u, requests.size());
  EXPECT_EQ(*timing.navigation_start, requests.at(1).period_start);
  EXPECT_EQ(*timing.frame_largest_contentful_paint, requests.at(1).period_end);
  EXPECT_EQ("PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.SubFrame",
            requests.at(1).name);
  EXPECT_EQ(20, requests.at(1).value);

  // If nothing modified, should not send any requests.
  recorder.UpdateMetadata(timing);
  EXPECT_EQ(2u, requests.size());
}

TEST_F(PageTimingMetadataRecorderTest,
       FrameLargestContentfulPaintInvalidDuration) {
  PageTimingMetadataRecorder::MonotonicTiming timing;
  TestPageTimingMetadataRecorder recorder(timing);
  const std::vector<MetadataTaggingRequest>& requests =
      recorder.GetMetadataTaggingRequests();

  timing.navigation_start = base::TimeTicks::Now();
  // Should reject negative TimeDelta.
  timing.frame_largest_contentful_paint =
      *timing.navigation_start - base::Hours(1);
  recorder.UpdateMetadata(timing);
  EXPECT_EQ(0u, requests.size());

  // Should accept zero TimeDelta.
  timing.frame_largest_contentful_paint = timing.navigation_start;
  recorder.UpdateMetadata(timing);
  EXPECT_EQ(1u, requests.size());
}

namespace {
uint32_t ExtractInstanceIdFromKey(int64_t key) {
  return static_cast<uint32_t>(static_cast<uint64_t>(key) >> 32);
}

uint32_t ExtractInteractionIdFromKey(int64_t key) {
  return static_cast<uint32_t>(static_cast<uint64_t>(key) & 0xffffffff);
}
}  // namespace

TEST_F(PageTimingMetadataRecorderTest,
       InteractionDurationMultipleInteractions) {
  PageTimingMetadataRecorder::MonotonicTiming timing;
  TestPageTimingMetadataRecorder recorder(timing);
  const std::vector<MetadataTaggingRequest>& requests =
      recorder.GetMetadataTaggingRequests();

  const base::TimeTicks time_origin = base::TimeTicks::Now();

  const base::TimeTicks interaction1_start =
      time_origin - base::Milliseconds(500);
  const base::TimeTicks interaction1_end =
      time_origin - base::Milliseconds(300);
  recorder.AddInteractionDurationMetadata(interaction1_start, interaction1_end);

  ASSERT_EQ(1u, requests.size());
  EXPECT_EQ(interaction1_start, requests.at(0).period_start);
  EXPECT_EQ(interaction1_end, requests.at(0).period_end);

  const base::TimeTicks interaction2_start =
      time_origin - base::Milliseconds(200);
  const base::TimeTicks interaction2_end =
      time_origin - base::Milliseconds(100);
  recorder.AddInteractionDurationMetadata(interaction2_start, interaction2_end);

  ASSERT_EQ(2u, requests.size());
  EXPECT_EQ(interaction2_start, requests.at(1).period_start);
  EXPECT_EQ(interaction2_end, requests.at(1).period_end);

  const int64_t key1 = requests.at(0).key;
  const int64_t key2 = requests.at(1).key;
  EXPECT_EQ(ExtractInstanceIdFromKey(key1), ExtractInstanceIdFromKey(key2));
  EXPECT_EQ(ExtractInteractionIdFromKey(key1), 1u);
  EXPECT_EQ(ExtractInteractionIdFromKey(key2), 2u);
}

TEST_F(PageTimingMetadataRecorderTest, InteractionDurationMultipleRecorders) {
  const base::TimeTicks time_origin = base::TimeTicks::Now();

  const base::TimeTicks interaction1_start =
      time_origin - base::Milliseconds(500);
  const base::TimeTicks interaction1_end =
      time_origin - base::Milliseconds(300);

  const base::TimeTicks interaction2_start =
      time_origin - base::Milliseconds(200);
  const base::TimeTicks interaction2_end =
      time_origin - base::Milliseconds(100);

  PageTimingMetadataRecorder::MonotonicTiming timing;
  TestPageTimingMetadataRecorder recorder1(timing);
  TestPageTimingMetadataRecorder recorder2(timing);

  recorder1.AddInteractionDurationMetadata(interaction1_start,
                                           interaction1_end);
  recorder2.AddInteractionDurationMetadata(interaction2_start,
                                           interaction2_end);

  const std::vector<MetadataTaggingRequest>& requests1 =
      recorder1.GetMetadataTaggingRequests();
  const std::vector<MetadataTaggingRequest>& requests2 =
      recorder2.GetMetadataTaggingRequests();

  ASSERT_EQ(1u, requests1.size());
  ASSERT_EQ(1u, requests2.size());

  const int64_t key1 = requests1.at(0).key;
  const int64_t key2 = requests2.at(0).key;
  EXPECT_NE(ExtractInstanceIdFromKey(key1), ExtractInstanceIdFromKey(key2));
  EXPECT_EQ(ExtractInteractionIdFromKey(key1), 1u);
  EXPECT_EQ(ExtractInteractionIdFromKey(key2), 1u);
}

TEST_F(PageTimingMetadataRecorderTest, InteractionDurationInvalidDuration) {
  PageTimingMetadataRecorder::MonotonicTiming timing;
  TestPageTimingMetadataRecorder recorder(timing);
  const std::vector<MetadataTaggingRequest>& requests =
      recorder.GetMetadataTaggingRequests();

  const base::TimeTicks time_origin = base::TimeTicks::Now();

  // End time cannot more than TimeTicks::Now().
  const base::TimeTicks interaction1_start =
      time_origin - base::Milliseconds(500);
  const base::TimeTicks interaction1_end = time_origin + base::Hours(500);
  recorder.AddInteractionDurationMetadata(interaction1_start, interaction1_end);
  EXPECT_EQ(0u, requests.size());

  // End time cannot be earlier than start time.
  const base::TimeTicks interaction2_start =
      time_origin - base::Milliseconds(500);
  const base::TimeTicks interaction2_end =
      time_origin - base::Milliseconds(501);
  recorder.AddInteractionDurationMetadata(interaction2_start, interaction2_end);
  EXPECT_EQ(0u, requests.size());
}

TEST_F(PageTimingMetadataRecorderTest,
       InteractionDurationMetadataKeySignednessTest) {
  {
    // Test that if instance_id has high bit set, interaction_id remains intact.
    const uint32_t instance_id = 0xffffffff;
    const uint32_t interaction_id = 1;
    const int64_t key =
        PageTimingMetadataRecorder::CreateInteractionDurationMetadataKey(
            instance_id, interaction_id);
    EXPECT_EQ(instance_id, ExtractInstanceIdFromKey(key));
    EXPECT_EQ(interaction_id, ExtractInteractionIdFromKey(key));
  }

  {
    // Test that if interaction_id has high bit set, instance_id remains intact.
    const uint32_t instance_id = 1;
    const uint32_t interaction_id = 0xffffffff;
    const int64_t key =
        PageTimingMetadataRecorder::CreateInteractionDurationMetadataKey(
            instance_id, interaction_id);
    EXPECT_EQ(instance_id, ExtractInstanceIdFromKey(key));
    EXPECT_EQ(interaction_id, ExtractInteractionIdFromKey(key));
  }
}

TEST_F(PageTimingMetadataRecorderTest,
       InteractionDurationQueuedDurationSucceed) {
  PageTimingMetadataRecorder::MonotonicTiming timing;
  TestPageTimingMetadataRecorder recorder(timing);
  const std::vector<MetadataTaggingRequest>& requests =
      recorder.GetMetadataTaggingRequests();

  const base::TimeTicks time_origin = base::TimeTicks::Now() - base::Seconds(1);

  const base::TimeTicks interaction1_start = time_origin;
  const base::TimeTicks interaction1_queued =
      time_origin + base::Milliseconds(200);
  const base::TimeTicks interaction1_commit_finished =
      time_origin + base::Milliseconds(300);
  const base::TimeTicks interaction1_end =
      time_origin + base::Milliseconds(500);
  recorder.AddInteractionDurationAfterQueueingMetadata(
      interaction1_start, interaction1_queued, interaction1_commit_finished,
      interaction1_end);

  ASSERT_EQ(1u, requests.size());
  EXPECT_EQ(interaction1_queued, requests.at(0).period_start);
  EXPECT_EQ(interaction1_commit_finished, requests.at(0).period_end);
}

TEST_F(PageTimingMetadataRecorderTest,
       InteractionDurationQueuedInvalidDurationRejected) {
  PageTimingMetadataRecorder::MonotonicTiming timing;
  TestPageTimingMetadataRecorder recorder(timing);
  const std::vector<MetadataTaggingRequest>& requests =
      recorder.GetMetadataTaggingRequests();

  const base::TimeTicks interaction_start =
      base::TimeTicks::Now() - base::Seconds(1);
  const base::TimeTicks interaction_end =
      interaction_start + base::Milliseconds(500);

  // Queued timestamp earlier than start is invalid.
  base::TimeTicks interaction_queued =
      interaction_start - base::Milliseconds(100);
  base::TimeTicks interaction_commit_finished =
      interaction_start + base::Milliseconds(200);
  recorder.AddInteractionDurationAfterQueueingMetadata(
      interaction_start, interaction_queued, interaction_commit_finished,
      interaction_end);

  ASSERT_EQ(0u, requests.size());

  // Commit finish timestamp before queued is invalid.
  interaction_queued = interaction_start + base::Milliseconds(200);
  interaction_commit_finished = interaction_start + base::Milliseconds(100);
  recorder.AddInteractionDurationAfterQueueingMetadata(
      interaction_start, interaction_queued, interaction_commit_finished,
      interaction_end);

  ASSERT_EQ(0u, requests.size());

  // Commit finish timestamp after end is invalid.
  interaction_queued = interaction_start + base::Milliseconds(100);
  interaction_commit_finished = interaction_start + base::Milliseconds(600);
  recorder.AddInteractionDurationAfterQueueingMetadata(
      interaction_start, interaction_queued, interaction_commit_finished,
      interaction_end);

  ASSERT_EQ(0u, requests.size());
}

TEST_F(PageTimingMetadataRecorderTest,
       InteractionDurationQueuedEmptyCommitFinishTimeFallbackToPresentation) {
  PageTimingMetadataRecorder::MonotonicTiming timing;
  TestPageTimingMetadataRecorder recorder(timing);
  const std::vector<MetadataTaggingRequest>& requests =
      recorder.GetMetadataTaggingRequests();

  const base::TimeTicks time_origin = base::TimeTicks::Now() - base::Seconds(1);

  const base::TimeTicks interaction1_start = time_origin;
  const base::TimeTicks interaction1_queued =
      time_origin + base::Milliseconds(200);
  const base::TimeTicks interaction1_commit_finished = base::TimeTicks();
  const base::TimeTicks interaction1_end =
      time_origin + base::Milliseconds(500);
  recorder.AddInteractionDurationAfterQueueingMetadata(
      interaction1_start, interaction1_queued, interaction1_commit_finished,
      interaction1_end);

  ASSERT_EQ(1u, requests.size());
  EXPECT_EQ(interaction1_queued, requests.at(0).period_start);
  EXPECT_EQ(interaction1_end, requests.at(0).period_end);
}

}  // namespace page_load_metrics
