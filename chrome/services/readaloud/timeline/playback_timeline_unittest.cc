// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/timeline/playback_timeline.h"

#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

void PrintTo(const TextChunk& chunk, std::ostream* os) {
  *os << "TextChunk{text=\"" << base::UTF16ToUTF8(chunk.text)
      << "\", start_code_unit_offset=" << chunk.start_code_unit_offset
      << ", speaker=" << chunk.speaker << "}";
}

namespace {

using ::testing::ElementsAre;

read_aloud::mojom::TextSegmentPtr MakeSegment(
    uint32_t index,
    std::u16string_view text,
    read_aloud::mojom::Speaker speaker =
        read_aloud::mojom::Speaker::kSpeaker1) {
  auto segment = read_aloud::mojom::TextSegment::New();
  segment->segment_index = index;
  segment->text = std::u16string(text);
  segment->speaker = speaker;
  return segment;
}

}  // namespace

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
  segments.push_back(MakeSegment(0, u"First sentence. Second sentence."));

  timeline_.SetTextContent(segments);
  EXPECT_THAT(timeline_.chunks(),
              ElementsAre(TextChunk{.text = u"First sentence.",
                                    .start_code_unit_offset = 0u},
                          TextChunk{.text = u"Second sentence.",
                                    .start_code_unit_offset = 16u}));

  timeline_.Clear();
  EXPECT_EQ(timeline_.GetChunkCount(), 0u);
  EXPECT_TRUE(timeline_.chunks().empty());
}

TEST_F(PlaybackTimelineTest, SetTextContentAcrossMultipleSegments) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  segments.push_back(MakeSegment(0, u"First sentence. "));
  segments.push_back(MakeSegment(1, u"Second sentence."));

  timeline_.SetTextContent(segments);
  EXPECT_THAT(timeline_.chunks(),
              ElementsAre(TextChunk{.text = u"First sentence.",
                                    .start_code_unit_offset = 0u},
                          TextChunk{.text = u"Second sentence.",
                                    .start_code_unit_offset = 16u}));
}

TEST_F(PlaybackTimelineTest, ClearResetsTimelineState) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  segments.push_back(MakeSegment(0, u"Sample sentence."));

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
  segments.push_back(MakeSegment(0, u"First sentence. Second sentence."));

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
  segments.push_back(MakeSegment(0, u"Short."));

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
  segments.push_back(MakeSegment(0, u"First sentence. Second sentence."));

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

TEST_F(PlaybackTimelineTest, ResolveSegmentOffsetAcrossSegments) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  segments.push_back(MakeSegment(0, u"Sentence one. "));
  segments.push_back(MakeSegment(1, u"Sentence two."));

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
  segments.push_back(MakeSegment(0, u"Sentence one. Sentence two."));

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
  segments.push_back(MakeSegment(0, u"One. Two. Three."));

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
  segments.push_back(MakeSegment(0, u"Sentence one. Sentence two."));

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

TEST_F(PlaybackTimelineTest, GetTotalDurationIsZeroWhenEmpty) {
  EXPECT_EQ(timeline_.GetTotalDuration(), base::TimeDelta());
}

TEST_F(PlaybackTimelineTest, GetTotalDurationIsCharacterEstimateBeforeUpdates) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  // Chunks: "First sentence." (15 chars), "Second sentence." (16 chars).
  segments.push_back(MakeSegment(0, u"First sentence. Second sentence."));

  timeline_.SetTextContent(segments);

  EXPECT_EQ(timeline_.GetTotalDuration(),
            31 * PlaybackTimeline::kEstimatedDurationPerChar);
}

TEST_F(PlaybackTimelineTest, GetTotalDurationFollowsUpdateSentenceDuration) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  // Chunks: "Sentence one." (13 chars), "Sentence two." (13 chars).
  segments.push_back(MakeSegment(0, u"Sentence one. Sentence two."));

  timeline_.SetTextContent(segments);
  timeline_.UpdateSentenceDuration(/*sentence_index=*/1,
                                   base::Milliseconds(1200));

  EXPECT_EQ(timeline_.GetTotalDuration(),
            13 * PlaybackTimeline::kEstimatedDurationPerChar +
                base::Milliseconds(1200));

  // A later update for the same chunk replaces the earlier one.
  timeline_.UpdateSentenceDuration(/*sentence_index=*/1,
                                   base::Milliseconds(700));

  EXPECT_EQ(timeline_.GetTotalDuration(),
            13 * PlaybackTimeline::kEstimatedDurationPerChar +
                base::Milliseconds(700));
}

TEST_F(PlaybackTimelineTest, ResolveTimeOffsetSnapsMidWordToWordStart) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  // Chunk 0: "First sentence." (15 chars)
  // Word 0: "First" [0, 5), Word 1: "sentence" [6, 14)
  segments.push_back(MakeSegment(0, u"First sentence. Second sentence."));

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
  segments.push_back(MakeSegment(0, u"Hello world."));

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
  segments.push_back(MakeSegment(0, u"First.   Second word."));

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
  segments.push_back(MakeSegment(0, u"Sentence."));

  timeline_.SetTextContent(segments);

  EXPECT_FALSE(timeline_.ResolveTimeOffset(base::Seconds(-1)).has_value());
  EXPECT_FALSE(timeline_.ResolveTimeOffset(base::TimeDelta::Max()).has_value());
}

TEST_F(PlaybackTimelineTest, ResolveTimeOffsetMidDocument) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  segments.push_back(MakeSegment(0, u"Sentence one. "));
  segments.push_back(MakeSegment(1, u"Sentence two is longer."));

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
  segments.push_back(MakeSegment(0, u"Chunk zero. Chunk one."));

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
  segments.push_back(MakeSegment(0, u"Only sentence."));

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
  segments.push_back(MakeSegment(0, u"Only sentence."));

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
  segments.push_back(MakeSegment(0, u"Sentence one. Sentence two."));

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

TEST_F(PlaybackTimelineTest,
       SetTextContentMultiSegmentMonotonicDocumentOffsets) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  segments.push_back(MakeSegment(0, u"First sentence. Second sentence! "));
  segments.push_back(MakeSegment(1, u"Third sentence? Fourth sentence."));

  timeline_.SetTextContent(segments, base::i18n::GetKnownLanguageTag("en-US"));

  EXPECT_THAT(
      timeline_.chunks(),
      ElementsAre(
          TextChunk{.text = u"First sentence.", .start_code_unit_offset = 0u},
          TextChunk{.text = u"Second sentence!", .start_code_unit_offset = 16u},
          TextChunk{.text = u"Third sentence?", .start_code_unit_offset = 33u},
          TextChunk{.text = u"Fourth sentence.",
                    .start_code_unit_offset = 49u}));
}

TEST_F(PlaybackTimelineTest, SetTextContentInterleavedNullSegment) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  segments.push_back(MakeSegment(0, u"First sentence."));  // Length 15
  // Interleaved null segment should be safely skipped.
  segments.push_back(nullptr);
  segments.push_back(MakeSegment(1, u"Second sentence."));  // Length 16

  timeline_.SetTextContent(segments, base::i18n::GetKnownLanguageTag("en-US"));

  EXPECT_THAT(timeline_.chunks(),
              ElementsAre(TextChunk{.text = u"First sentence.",
                                    .start_code_unit_offset = 0u},
                          TextChunk{.text = u"Second sentence.",
                                    .start_code_unit_offset = 16u}));
}

TEST_F(PlaybackTimelineTest, SetTextContentInterleavedEmptySegment) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  segments.push_back(MakeSegment(0, u"First sentence."));  // Length 15
  // Interleaved empty segment should be safely skipped without accumulating.
  segments.push_back(MakeSegment(1, u""));
  segments.push_back(MakeSegment(2, u"Second sentence."));  // Length 16

  timeline_.SetTextContent(segments, base::i18n::GetKnownLanguageTag("en-US"));

  EXPECT_THAT(timeline_.chunks(),
              ElementsAre(TextChunk{.text = u"First sentence.",
                                    .start_code_unit_offset = 0u},
                          TextChunk{.text = u"Second sentence.",
                                    .start_code_unit_offset = 16u}));
}

// Chunk offsets live in the highlighter's coordinate space: the trimmed chunks
// joined by exactly one separator. Whitespace trimmed by the chunker, including
// whole whitespace-only segments, must not contribute.
TEST_F(PlaybackTimelineTest, SetTextContentAssignsJoinedTrimmedOffsets) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  segments.push_back(MakeSegment(0, u"  First sentence.   Second one!  "));
  segments.push_back(MakeSegment(1, u"   \t\n   "));
  segments.push_back(MakeSegment(2, u"\nThird."));

  timeline_.SetTextContent(segments, base::i18n::GetKnownLanguageTag("en-US"));

  EXPECT_THAT(
      timeline_.chunks(),
      ElementsAre(
          TextChunk{.text = u"First sentence.", .start_code_unit_offset = 0u},
          // len("First sentence.") + 1 separator.
          TextChunk{.text = u"Second one!", .start_code_unit_offset = 16u},
          // 16 + len("Second one!") + 1 separator.
          TextChunk{.text = u"Third.", .start_code_unit_offset = 28u}));
}

TEST_F(PlaybackTimelineTest, SetTextContentPreservesSpeakerPerSegment) {
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  segments.push_back(MakeSegment(0,
                                 u"Host first sentence. Host second sentence.",
                                 read_aloud::mojom::Speaker::kSpeaker1));
  segments.push_back(MakeSegment(1, u"Guest answering.",
                                 read_aloud::mojom::Speaker::kSpeaker2));

  timeline_.SetTextContent(segments, base::i18n::GetKnownLanguageTag("en-US"));

  EXPECT_THAT(
      timeline_.chunks(),
      ElementsAre(TextChunk{.text = u"Host first sentence.",
                            .start_code_unit_offset = 0u,
                            .speaker = read_aloud::mojom::Speaker::kSpeaker1},
                  TextChunk{.text = u"Host second sentence.",
                            .start_code_unit_offset = 21u,
                            .speaker = read_aloud::mojom::Speaker::kSpeaker1},
                  TextChunk{.text = u"Guest answering.",
                            .start_code_unit_offset = 43u,
                            .speaker = read_aloud::mojom::Speaker::kSpeaker2}));
}

}  // namespace readaloud
