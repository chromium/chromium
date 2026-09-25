// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/timeline/playback_timeline.h"

#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

class PlaybackTimelineTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  PlaybackTimeline timeline_;
};

TEST_F(PlaybackTimelineTest, DefaultConstructorIsEmpty) {
  EXPECT_EQ(timeline_.GetChunkCount(), 0u);
  EXPECT_TRUE(timeline_.chunks().empty());
}

TEST_F(PlaybackTimelineTest, SetTextContentPopulatesChunks) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg0 = read_aloud::mojom::TextSegment::New();
  seg0->segment_index = 0;
  seg0->text = u"First sentence. Second sentence.";
  segments.push_back(std::move(seg0));

  timeline_.SetTextContent(segments);
  EXPECT_EQ(timeline_.GetChunkCount(), 2u);
  EXPECT_EQ(timeline_.chunks()[0].text, u"First sentence.");
  EXPECT_EQ(timeline_.chunks()[0].start_code_unit_offset, 0u);
  EXPECT_EQ(timeline_.chunks()[1].text, u"Second sentence.");
  EXPECT_EQ(timeline_.chunks()[1].start_code_unit_offset, 16u);

  timeline_.Clear();
  EXPECT_EQ(timeline_.GetChunkCount(), 0u);
  EXPECT_TRUE(timeline_.chunks().empty());
}

TEST_F(PlaybackTimelineTest,
       SetTextContentConcatenatesMultipleSegmentsIntoSingleDocument) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  {
    auto seg0 = read_aloud::mojom::TextSegment::New();
    seg0->segment_index = 0;
    seg0->text = u"First sentence. ";
    segments.push_back(std::move(seg0));
  }
  {
    auto seg1 = read_aloud::mojom::TextSegment::New();
    seg1->segment_index = 1;
    seg1->text = u"Second sentence.";
    segments.push_back(std::move(seg1));
  }

  timeline_.SetTextContent(segments);
  EXPECT_EQ(timeline_.GetChunkCount(), 2u);
  EXPECT_EQ(timeline_.chunks()[0].text, u"First sentence.");
  EXPECT_EQ(timeline_.chunks()[0].start_code_unit_offset, 0u);
  EXPECT_EQ(timeline_.chunks()[1].text, u"Second sentence.");
  EXPECT_EQ(timeline_.chunks()[1].start_code_unit_offset, 16u);
}

TEST_F(PlaybackTimelineTest, ClearResetsTimelineState) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg0 = read_aloud::mojom::TextSegment::New();
  seg0->segment_index = 0;
  seg0->text = u"Sample sentence.";
  segments.push_back(std::move(seg0));

  timeline_.SetTextContent(segments);
  EXPECT_EQ(timeline_.GetChunkCount(), 1u);

  timeline_.Clear();
  EXPECT_EQ(timeline_.GetChunkCount(), 0u);
  EXPECT_TRUE(timeline_.chunks().empty());
}

TEST_F(PlaybackTimelineTest, TimelinePositionStructDefaultsAndHelpers) {
  TimelinePosition pos1;
  EXPECT_EQ(pos1.chunk.index, 0u);
  EXPECT_EQ(pos1.chunk.start_char_offset, 0u);
  EXPECT_EQ(pos1.chunk.end_char_offset, 0u);

  EXPECT_EQ(pos1.global_char.start_offset, 0u);
  EXPECT_EQ(pos1.global_char.end_offset, 0u);

  EXPECT_EQ(pos1.time.start_time, base::TimeDelta());
  EXPECT_EQ(pos1.time.end_time, base::TimeDelta());
  EXPECT_EQ(pos1.time.duration(), base::TimeDelta());

  pos1.time.start_time = base::Milliseconds(100);
  pos1.time.end_time = base::Milliseconds(350);
  EXPECT_EQ(pos1.time.duration(), base::Milliseconds(250));

  TextChunk chunk{.text = u"Hello world.", .start_code_unit_offset = 20u};
  TimelinePosition pos2(/*chunk_index=*/2u, chunk,
                        /*char_offset_in_chunk=*/6u,
                        /*start_time=*/base::Milliseconds(100),
                        /*end_time=*/base::Milliseconds(350));
  EXPECT_EQ(pos2.chunk.index, 2u);
  EXPECT_EQ(pos2.chunk.start_char_offset, 6u);
  EXPECT_EQ(pos2.chunk.end_char_offset, 12u);
  EXPECT_EQ(pos2.global_char.start_offset, 26u);
  EXPECT_EQ(pos2.global_char.end_offset, 32u);
  EXPECT_EQ(pos2.time.duration(), base::Milliseconds(250));
}

TEST_F(PlaybackTimelineTest, ResolveSegmentOffsetValid) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg0 = read_aloud::mojom::TextSegment::New();
  seg0->segment_index = 0;
  seg0->text = u"First sentence. Second sentence.";
  segments.push_back(std::move(seg0));

  timeline_.SetTextContent(segments);

  std::optional<TimelinePosition> pos0 = timeline_.ResolveSegmentOffset(
      /*segment_index=*/0, /*character_offset=*/0);
  ASSERT_TRUE(pos0.has_value());
  EXPECT_EQ(pos0->chunk.index, 0u);
  EXPECT_EQ(pos0->chunk.start_char_offset, 0u);
  EXPECT_EQ(pos0->chunk.end_char_offset, 15u);
  EXPECT_EQ(pos0->global_char.start_offset, 0u);
  EXPECT_EQ(pos0->global_char.end_offset, 15u);
  EXPECT_EQ(pos0->time.start_time, base::Milliseconds(0));
  EXPECT_EQ(pos0->time.end_time,
            15 * PlaybackTimeline::kEstimatedDurationPerChar);

  std::optional<TimelinePosition> pos1 = timeline_.ResolveSegmentOffset(
      /*segment_index=*/1, /*character_offset=*/0);
  ASSERT_TRUE(pos1.has_value());
  EXPECT_EQ(pos1->chunk.index, 1u);
  EXPECT_EQ(pos1->chunk.start_char_offset, 0u);
  EXPECT_EQ(pos1->chunk.end_char_offset, 16u);
  EXPECT_EQ(pos1->global_char.start_offset, 16u);
  EXPECT_EQ(pos1->global_char.end_offset, 32u);
  EXPECT_EQ(pos1->time.start_time,
            15 * PlaybackTimeline::kEstimatedDurationPerChar);
  EXPECT_EQ(pos1->time.end_time,
            31 * PlaybackTimeline::kEstimatedDurationPerChar);
}

TEST_F(PlaybackTimelineTest, ResolveSegmentOffsetOutOfBounds) {
  EXPECT_FALSE(timeline_
                   .ResolveSegmentOffset(/*segment_index=*/0,
                                         /*character_offset=*/0)
                   .has_value());

  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg0 = read_aloud::mojom::TextSegment::New();
  seg0->segment_index = 0;
  seg0->text = u"Short.";
  segments.push_back(std::move(seg0));

  timeline_.SetTextContent(segments);

  EXPECT_FALSE(timeline_
                   .ResolveSegmentOffset(/*segment_index=*/1,
                                         /*character_offset=*/0)
                   .has_value());
  EXPECT_FALSE(timeline_
                   .ResolveSegmentOffset(/*segment_index=*/0,
                                         /*character_offset=*/7)
                   .has_value());
}

TEST_F(PlaybackTimelineTest, ResolveSegmentOffsetMidSentence) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg0 = read_aloud::mojom::TextSegment::New();
  seg0->segment_index = 0;
  seg0->text = u"First sentence. Second sentence.";
  segments.push_back(std::move(seg0));

  timeline_.SetTextContent(segments);

  std::optional<TimelinePosition> pos = timeline_.ResolveSegmentOffset(
      /*segment_index=*/1, /*character_offset=*/7);
  ASSERT_TRUE(pos.has_value());
  EXPECT_EQ(pos->chunk.index, 1u);
  EXPECT_EQ(pos->chunk.start_char_offset, 7u);
  EXPECT_EQ(pos->chunk.end_char_offset, 16u);
  EXPECT_EQ(pos->global_char.start_offset, 23u);
  EXPECT_EQ(pos->global_char.end_offset, 32u);
  EXPECT_EQ(pos->time.start_time,
            15 * PlaybackTimeline::kEstimatedDurationPerChar);
  EXPECT_EQ(pos->time.end_time,
            31 * PlaybackTimeline::kEstimatedDurationPerChar);
}

TEST_F(PlaybackTimelineTest,
       ResolveSegmentOffsetAcrossConcatenatedSegmentBoundaries) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  {
    auto seg0 = read_aloud::mojom::TextSegment::New();
    seg0->segment_index = 0;
    seg0->text = u"Sentence one. ";
    segments.push_back(std::move(seg0));
  }
  {
    auto seg1 = read_aloud::mojom::TextSegment::New();
    seg1->segment_index = 1;
    seg1->text = u"Sentence two.";
    segments.push_back(std::move(seg1));
  }

  timeline_.SetTextContent(segments);

  std::optional<TimelinePosition> pos = timeline_.ResolveSegmentOffset(
      /*segment_index=*/1, /*character_offset=*/0);
  ASSERT_TRUE(pos.has_value());
  EXPECT_EQ(pos->chunk.index, 1u);
  EXPECT_EQ(pos->chunk.start_char_offset, 0u);
  EXPECT_EQ(pos->chunk.end_char_offset, 13u);
  EXPECT_EQ(pos->global_char.start_offset, 14u);
  EXPECT_EQ(pos->global_char.end_offset, 27u);
  EXPECT_EQ(pos->time.start_time,
            13 * PlaybackTimeline::kEstimatedDurationPerChar);
  EXPECT_EQ(pos->time.end_time,
            26 * PlaybackTimeline::kEstimatedDurationPerChar);
}

TEST_F(PlaybackTimelineTest,
       UpdateSentenceDurationAdjustsSubsequentChunkTimeOffsets) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"Sentence one. Sentence two.";
  segments.push_back(std::move(seg));

  timeline_.SetTextContent(segments);

  // Update chunk 0 actual duration to 1200ms (est was
  // 13 * kEstimatedDurationPerChar = 845ms, deviation = +355ms).
  timeline_.UpdateSentenceDuration(/*sentence_index=*/0,
                                   base::Milliseconds(1200));

  std::optional<TimelinePosition> pos0 = timeline_.ResolveSegmentOffset(
      /*segment_index=*/0, /*character_offset=*/0);
  ASSERT_TRUE(pos0.has_value());
  EXPECT_EQ(pos0->chunk.index, 0u);
  EXPECT_EQ(pos0->time.start_time, base::Milliseconds(0));
  EXPECT_EQ(pos0->time.end_time, base::Milliseconds(1200));

  std::optional<TimelinePosition> pos1 = timeline_.ResolveSegmentOffset(
      /*segment_index=*/1, /*character_offset=*/0);
  ASSERT_TRUE(pos1.has_value());
  EXPECT_EQ(pos1->chunk.index, 1u);
  EXPECT_EQ(pos1->time.start_time, base::Milliseconds(1200));
  EXPECT_EQ(pos1->time.end_time,
            base::Milliseconds(1200) +
                13 * PlaybackTimeline::kEstimatedDurationPerChar);
}

TEST_F(PlaybackTimelineTest, UpdateSentenceDurationMultipleChunks) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"One. Two. Three.";
  segments.push_back(std::move(seg));

  timeline_.SetTextContent(segments);

  timeline_.UpdateSentenceDuration(/*sentence_index=*/0,
                                   base::Milliseconds(400));
  timeline_.UpdateSentenceDuration(/*sentence_index=*/1,
                                   base::Milliseconds(700));

  std::optional<TimelinePosition> pos2 = timeline_.ResolveSegmentOffset(
      /*segment_index=*/2, /*character_offset=*/0);
  ASSERT_TRUE(pos2.has_value());
  EXPECT_EQ(pos2->chunk.index, 2u);
  EXPECT_EQ(pos2->time.start_time, base::Milliseconds(1100));
  EXPECT_EQ(pos2->time.end_time,
            base::Milliseconds(1100) +
                6 * PlaybackTimeline::kEstimatedDurationPerChar);
}

TEST_F(PlaybackTimelineTest, UpdateSentenceDurationOverwritesPreviousDeviation) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"Sentence one. Sentence two.";
  segments.push_back(std::move(seg));

  timeline_.SetTextContent(segments);

  timeline_.UpdateSentenceDuration(/*sentence_index=*/0,
                                   base::Milliseconds(500));
  timeline_.UpdateSentenceDuration(/*sentence_index=*/0,
                                   base::Milliseconds(1000));

  std::optional<TimelinePosition> pos1 = timeline_.ResolveSegmentOffset(
      /*segment_index=*/1, /*character_offset=*/0);
  ASSERT_TRUE(pos1.has_value());
  EXPECT_EQ(pos1->chunk.index, 1u);
  EXPECT_EQ(pos1->time.start_time, base::Milliseconds(1000));
}

}  // namespace readaloud
