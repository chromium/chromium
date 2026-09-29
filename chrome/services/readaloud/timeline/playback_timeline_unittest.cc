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

TEST_F(PlaybackTimelineTest, ResolveTimeOffsetSnapsMidWordToWordStart) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg0 = read_aloud::mojom::TextSegment::New();
  seg0->segment_index = 0;
  // Chunk 0: "First sentence." (15 chars)
  // Word 0: "First" [0, 5), Word 1: "sentence" [6, 14)
  seg0->text = u"First sentence. Second sentence.";
  segments.push_back(std::move(seg0));

  timeline_.SetTextContent(segments);

  // 3 * kEstimatedDurationPerChar corresponds to raw char index 3 ('s' inside
  // "First"). Word-boundary snapping snaps back to char offset 0 ("First").
  const base::TimeDelta kMidFirstWordTime =
      3 * PlaybackTimeline::kEstimatedDurationPerChar;
  std::optional<TimelinePosition> pos_word0 =
      timeline_.ResolveTimeOffset(kMidFirstWordTime);
  ASSERT_TRUE(pos_word0.has_value());
  EXPECT_EQ(pos_word0->chunk.index, 0u);
  EXPECT_EQ(pos_word0->chunk.start_char_offset, 0u);
  EXPECT_EQ(pos_word0->global_char.start_offset, 0u);
  EXPECT_EQ(pos_word0->time.start_time, kMidFirstWordTime);
  EXPECT_EQ(pos_word0->time.end_time,
            15 * PlaybackTimeline::kEstimatedDurationPerChar);

  // 9 * kEstimatedDurationPerChar corresponds to raw char index 9 ('t' inside
  // "sentence"). Word-boundary snapping snaps back to char offset 6
  // ("sentence").
  const base::TimeDelta kMidSecondWordTime =
      9 * PlaybackTimeline::kEstimatedDurationPerChar;
  std::optional<TimelinePosition> pos_word1 =
      timeline_.ResolveTimeOffset(kMidSecondWordTime);
  ASSERT_TRUE(pos_word1.has_value());
  EXPECT_EQ(pos_word1->chunk.index, 0u);
  EXPECT_EQ(pos_word1->chunk.start_char_offset, 6u);
  EXPECT_EQ(pos_word1->global_char.start_offset, 6u);
  EXPECT_EQ(pos_word1->time.start_time, kMidSecondWordTime);
  EXPECT_EQ(pos_word1->time.end_time,
            15 * PlaybackTimeline::kEstimatedDurationPerChar);
}

TEST_F(PlaybackTimelineTest, ResolveTimeOffsetUsesSynthesizedWordTimings) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg0 = read_aloud::mojom::TextSegment::New();
  seg0->segment_index = 0;
  seg0->text = u"Hello world.";
  segments.push_back(std::move(seg0));

  timeline_.SetTextContent(segments);

  std::vector<WordTiming> timings = {
      {.start_time = base::Milliseconds(0),
       .end_time = base::Milliseconds(700),
       .start_character_offset = 0u,
       .end_character_offset = 5u},
      {.start_time = base::Milliseconds(700),
       .end_time = base::Milliseconds(1100),
       .start_character_offset = 6u,
       .end_character_offset = 11u},
  };
  timeline_.UpdateSentenceDuration(/*sentence_index=*/0,
                                   base::Milliseconds(1100), timings);

  // At 600ms, linear interpolation would have landed in "world" (600/1100*12=6),
  // but actual WordTimings state "Hello" is spoken from [0ms, 700ms).
  std::optional<TimelinePosition> pos_hello =
      timeline_.ResolveTimeOffset(base::Milliseconds(600));
  ASSERT_TRUE(pos_hello.has_value());
  EXPECT_EQ(pos_hello->chunk.index, 0u);
  EXPECT_EQ(pos_hello->chunk.start_char_offset, 0u);
  EXPECT_EQ(pos_hello->global_char.start_offset, 0u);

  // At 800ms, WordTimings state "world" [6, 11) is spoken from [700ms, 1100ms).
  std::optional<TimelinePosition> pos_world =
      timeline_.ResolveTimeOffset(base::Milliseconds(800));
  ASSERT_TRUE(pos_world.has_value());
  EXPECT_EQ(pos_world->chunk.index, 0u);
  EXPECT_EQ(pos_world->chunk.start_char_offset, 6u);
  EXPECT_EQ(pos_world->global_char.start_offset, 6u);
}

// Word timings carry offsets in the same space as `start_code_unit_offset`
// (trimmed chunks joined by one separator), so the in-chunk offset is still
// recovered correctly for a later chunk whose source had extra whitespace.
TEST_F(PlaybackTimelineTest,
       ResolveTimeOffsetUsesWordTimingsInLaterChunkWithCollapsedWhitespace) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg0 = read_aloud::mojom::TextSegment::New();
  seg0->segment_index = 0;
  seg0->text = u"First.   Second word.";
  segments.push_back(std::move(seg0));

  timeline_.SetTextContent(segments);
  ASSERT_EQ(timeline_.GetChunkCount(), 2u);
  // The three spaces count as one separator: len("First.") + 1.
  EXPECT_EQ(timeline_.chunks()[1].start_code_unit_offset, 7u);

  std::vector<WordTiming> timings = {
      {.start_time = base::Milliseconds(0),
       .end_time = base::Milliseconds(300),
       .start_character_offset = 7u,
       .end_character_offset = 13u},
      {.start_time = base::Milliseconds(300),
       .end_time = base::Milliseconds(600),
       .start_character_offset = 14u,
       .end_character_offset = 18u},
  };
  timeline_.UpdateSentenceDuration(/*sentence_index=*/1,
                                   base::Milliseconds(600), timings);

  // 400 ms into the second chunk falls inside "word". Chunk 0 ("First.") keeps
  // its estimated duration.
  std::optional<TimelinePosition> pos = timeline_.ResolveTimeOffset(
      6 * PlaybackTimeline::kEstimatedDurationPerChar +
      base::Milliseconds(400));
  ASSERT_TRUE(pos.has_value());
  EXPECT_EQ(pos->chunk.index, 1u);
  EXPECT_EQ(pos->chunk.start_char_offset, 7u);
  EXPECT_EQ(pos->global_char.start_offset, 14u);
}

TEST_F(PlaybackTimelineTest, ResolveTimeOffsetNegativeOrMaxReturnsNullopt) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg0 = read_aloud::mojom::TextSegment::New();
  seg0->segment_index = 0;
  seg0->text = u"Sentence.";
  segments.push_back(std::move(seg0));

  timeline_.SetTextContent(segments);

  EXPECT_FALSE(timeline_.ResolveTimeOffset(base::Seconds(-1)).has_value());
  EXPECT_FALSE(timeline_.ResolveTimeOffset(base::TimeDelta::Max()).has_value());
}

TEST_F(PlaybackTimelineTest, ResolveTimeOffsetMidDocument) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  {
    auto seg = read_aloud::mojom::TextSegment::New();
    seg->segment_index = 0;
    seg->text = u"Sentence one. ";
    segments.push_back(std::move(seg));
  }
  {
    auto seg = read_aloud::mojom::TextSegment::New();
    seg->segment_index = 1;
    seg->text = u"Sentence two is longer.";
    segments.push_back(std::move(seg));
  }

  timeline_.SetTextContent(segments);

  // Chunk 0 ("Sentence one.") has 13 chars.
  // Seeking 11 chars into Chunk 1 ("Sentence two is longer.", global start 14)
  // lands on raw char 11 ('o' in "two"), which snaps to the start of word "two"
  // at chunk offset 9 (global offset 14 + 9 = 23).
  const base::TimeDelta kTargetTime =
      (13 + 11) * PlaybackTimeline::kEstimatedDurationPerChar;
  std::optional<TimelinePosition> pos =
      timeline_.ResolveTimeOffset(kTargetTime);
  ASSERT_TRUE(pos.has_value());
  EXPECT_EQ(pos->chunk.index, 1u);
  EXPECT_EQ(pos->chunk.start_char_offset, 9u);
  EXPECT_EQ(pos->global_char.start_offset, 23u);
  EXPECT_EQ(pos->time.start_time, kTargetTime);
}

TEST_F(PlaybackTimelineTest, ResolveTimeOffsetExactBoundarySnapping) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"Chunk zero. Chunk one.";
  segments.push_back(std::move(seg));

  timeline_.SetTextContent(segments);

  // Chunk 0 ("Chunk zero.", 11 chars) ends at 11 * kEstimatedDurationPerChar.
  std::optional<TimelinePosition> pos = timeline_.ResolveTimeOffset(
      11 * PlaybackTimeline::kEstimatedDurationPerChar);
  ASSERT_TRUE(pos.has_value());
  EXPECT_EQ(pos->chunk.index, 1u);
  EXPECT_EQ(pos->chunk.start_char_offset, 0u);
  EXPECT_EQ(pos->global_char.start_offset, 12u);
  EXPECT_EQ(pos->time.start_time,
            11 * PlaybackTimeline::kEstimatedDurationPerChar);
}

TEST_F(PlaybackTimelineTest, ResolveTimeOffsetAtExactDocumentEndBoundary) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"Only sentence.";
  segments.push_back(std::move(seg));

  timeline_.SetTextContent(segments);

  // "Only sentence." has 14 chars.
  std::optional<TimelinePosition> pos_end = timeline_.ResolveTimeOffset(
      14 * PlaybackTimeline::kEstimatedDurationPerChar);
  ASSERT_TRUE(pos_end.has_value());
  EXPECT_EQ(pos_end->chunk.index, 0u);
  EXPECT_EQ(pos_end->chunk.start_char_offset, 14u);
  EXPECT_EQ(pos_end->global_char.start_offset, 14u);
}

TEST_F(PlaybackTimelineTest, ResolveTimeOffsetBeyondDocumentEndClampsToEnd) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"Only sentence.";
  segments.push_back(std::move(seg));

  timeline_.SetTextContent(segments);

  std::optional<TimelinePosition> pos_beyond = timeline_.ResolveTimeOffset(
      (14 + 10) * PlaybackTimeline::kEstimatedDurationPerChar);
  ASSERT_TRUE(pos_beyond.has_value());
  EXPECT_EQ(pos_beyond->chunk.index, 0u);
  EXPECT_EQ(pos_beyond->chunk.start_char_offset, 14u);
  EXPECT_EQ(pos_beyond->global_char.start_offset, 14u);
}

TEST_F(PlaybackTimelineTest, ResolveTimeOffsetWithDeviations) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"Sentence one. Sentence two.";
  segments.push_back(std::move(seg));

  timeline_.SetTextContent(segments);

  // Baseline for Chunk 0 ("Sentence one.", 13 chars) is
  // 13 * kEstimatedDurationPerChar. Without any duration deviation,
  // 16 * kEstimatedDurationPerChar would land inside Chunk 1
  // ([13 * kEstimatedDurationPerChar, 26 * kEstimatedDurationPerChar)).
  // Updating Chunk 0's duration to 20 * kEstimatedDurationPerChar (+7 chars of
  // deviation) extends Chunk 0 to [0, 20 * kEstimatedDurationPerChar) and
  // shifts Chunk 1 to [20 * kEstimatedDurationPerChar,
  // 33 * kEstimatedDurationPerChar), causing 16 * kEstimatedDurationPerChar to
  // resolve within Chunk 0 instead.
  timeline_.UpdateSentenceDuration(
      /*sentence_index=*/0, 20 * PlaybackTimeline::kEstimatedDurationPerChar);

  const base::TimeDelta kTimeInExtendedChunk0 =
      16 * PlaybackTimeline::kEstimatedDurationPerChar;
  std::optional<TimelinePosition> pos0 =
      timeline_.ResolveTimeOffset(kTimeInExtendedChunk0);
  ASSERT_TRUE(pos0.has_value());
  EXPECT_EQ(pos0->chunk.index, 0u);
  EXPECT_EQ(pos0->time.start_time, kTimeInExtendedChunk0);

  const base::TimeDelta kTimeInShiftedChunk1 =
      22 * PlaybackTimeline::kEstimatedDurationPerChar;
  std::optional<TimelinePosition> pos1 =
      timeline_.ResolveTimeOffset(kTimeInShiftedChunk1);
  ASSERT_TRUE(pos1.has_value());
  EXPECT_EQ(pos1->chunk.index, 1u);
  EXPECT_EQ(pos1->time.start_time, kTimeInShiftedChunk1);
}

}  // namespace readaloud
