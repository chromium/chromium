// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/audio_renderer/word_boundary_queue.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "chrome/services/readaloud/word_timing.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

namespace {

using DueWord = WordBoundaryQueue::DueWord;

constexpr base::TimeDelta kMaxExtrapolation =
    WordBoundaryQueue::kMaxExtrapolation;

WordTiming MakeWord(base::TimeDelta start_time,
                    uint32_t start_character_offset,
                    uint32_t end_character_offset) {
  WordTiming word;
  word.start_time = start_time;
  word.end_time = start_time + base::Milliseconds(1);
  word.start_character_offset = start_character_offset;
  word.end_character_offset = end_character_offset;
  return word;
}

// Returns `word` moved `offset` later in media time.
WordTiming Shifted(WordTiming word, base::TimeDelta offset) {
  word.start_time += offset;
  word.end_time += offset;
  return word;
}

class WordBoundaryQueueTest : public testing::Test {
 protected:
  // Arbitrary wall time at which the tests anchor the queue.
  const base::TimeTicks anchor_wall_time_ =
      base::TimeTicks() + base::Seconds(100);
  WordBoundaryQueue queue_;
};

TEST_F(WordBoundaryQueueTest, EmptyQueuePopsNothing) {
  queue_.SetAudibleAnchor(/*media_time=*/base::TimeDelta(), anchor_wall_time_);

  EXPECT_EQ(queue_.PopDueWord(anchor_wall_time_, /*rate=*/1.0), std::nullopt);
}

TEST_F(WordBoundaryQueueTest, EmptyQueueHasNothingToWaitFor) {
  queue_.SetAudibleAnchor(/*media_time=*/base::TimeDelta(), anchor_wall_time_);

  EXPECT_EQ(queue_.TimeUntilNextWordDue(anchor_wall_time_, /*rate=*/1.0),
            std::nullopt);
}

TEST_F(WordBoundaryQueueTest, PopsWordAudibleAtAnchor) {
  const WordTiming word = MakeWord(/*start_time=*/base::TimeDelta(),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));
  queue_.SetAudibleAnchor(/*media_time=*/base::TimeDelta(), anchor_wall_time_);

  EXPECT_EQ(queue_.PopDueWord(anchor_wall_time_, /*rate=*/1.0),
            (DueWord{.timing = word, .audio_timestamp = base::TimeDelta()}));
  EXPECT_TRUE(queue_.empty());
}

TEST_F(WordBoundaryQueueTest, DoesNotPopWordBeforeItIsAudible) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(30),
                                   /*start_character_offset=*/6,
                                   /*end_character_offset=*/11);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(40));
  queue_.SetAudibleAnchor(/*media_time=*/base::Milliseconds(20),
                          anchor_wall_time_);

  // The word becomes audible 10 ms after the anchor.
  EXPECT_EQ(queue_.PopDueWord(anchor_wall_time_ + base::Milliseconds(10) -
                                  base::Microseconds(1),
                              /*rate=*/1.0),
            std::nullopt);
  EXPECT_FALSE(queue_.empty());
}

TEST_F(WordBoundaryQueueTest, TimeUntilNextWordDueIsRelativeToAnchor) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(30),
                                   /*start_character_offset=*/6,
                                   /*end_character_offset=*/11);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(40));
  queue_.SetAudibleAnchor(/*media_time=*/base::Milliseconds(20),
                          anchor_wall_time_);

  EXPECT_EQ(queue_.TimeUntilNextWordDue(anchor_wall_time_, /*rate=*/1.0),
            base::Milliseconds(10));
}

TEST_F(WordBoundaryQueueTest, TimeUntilNextWordDueIsZeroForOverdueWord) {
  const WordTiming word = MakeWord(/*start_time=*/base::TimeDelta(),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));
  queue_.SetAudibleAnchor(/*media_time=*/base::TimeDelta(), anchor_wall_time_);

  EXPECT_EQ(
      queue_.TimeUntilNextWordDue(anchor_wall_time_ + base::Milliseconds(5),
                                  /*rate=*/1.0),
      base::TimeDelta());
}

TEST_F(WordBoundaryQueueTest, PopsWordOnceAudibleRelativeToAnchor) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(30),
                                   /*start_character_offset=*/6,
                                   /*end_character_offset=*/11);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(40));
  queue_.SetAudibleAnchor(/*media_time=*/base::Milliseconds(20),
                          anchor_wall_time_);

  EXPECT_EQ(
      queue_.PopDueWord(anchor_wall_time_ + base::Milliseconds(10),
                        /*rate=*/1.0),
      (DueWord{.timing = word, .audio_timestamp = base::Milliseconds(30)}));
}

TEST_F(WordBoundaryQueueTest, ShiftsTimingsByDurationEnqueuedBefore) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(1),
                                   /*start_character_offset=*/7,
                                   /*end_character_offset=*/9);
  queue_.Enqueue(/*timings=*/{}, /*duration=*/base::Milliseconds(10));
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));
  queue_.SetAudibleAnchor(/*media_time=*/base::TimeDelta(), anchor_wall_time_);

  // 1 ms into the second segment is 11 ms into the media timeline.
  EXPECT_EQ(queue_.PopDueWord(anchor_wall_time_ + base::Milliseconds(11),
                              /*rate=*/1.0),
            (DueWord{.timing = Shifted(word, base::Milliseconds(10)),
                     .audio_timestamp = base::Milliseconds(11)}));
}

TEST_F(WordBoundaryQueueTest,
       ShiftsTimingsByTotalDurationOfAllEarlierSegments) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(1),
                                   /*start_character_offset=*/7,
                                   /*end_character_offset=*/9);
  queue_.Enqueue(/*timings=*/{}, /*duration=*/base::Milliseconds(10));
  queue_.Enqueue(/*timings=*/{}, /*duration=*/base::Milliseconds(20));
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));
  queue_.SetAudibleAnchor(/*media_time=*/base::TimeDelta(), anchor_wall_time_);

  // 1 ms into the third segment is 31 ms into the media timeline.
  EXPECT_EQ(queue_.PopDueWord(anchor_wall_time_ + base::Milliseconds(31),
                              /*rate=*/1.0),
            (DueWord{.timing = Shifted(word, base::Milliseconds(30)),
                     .audio_timestamp = base::Milliseconds(31)}));
}

TEST_F(WordBoundaryQueueTest, PopsDueWordsInOrder) {
  const std::vector<WordTiming> words = {
      MakeWord(/*start_time=*/base::Milliseconds(0),
               /*start_character_offset=*/0,
               /*end_character_offset=*/5),
      MakeWord(/*start_time=*/base::Milliseconds(5),
               /*start_character_offset=*/6,
               /*end_character_offset=*/11)};
  queue_.Enqueue(words, /*duration=*/base::Milliseconds(10));
  queue_.SetAudibleAnchor(/*media_time=*/base::TimeDelta(), anchor_wall_time_);
  const base::TimeTicks now = anchor_wall_time_ + base::Milliseconds(5);

  EXPECT_EQ(
      queue_.PopDueWord(now, /*rate=*/1.0),
      (DueWord{.timing = words[0], .audio_timestamp = base::Milliseconds(5)}));
  EXPECT_EQ(
      queue_.PopDueWord(now, /*rate=*/1.0),
      (DueWord{.timing = words[1], .audio_timestamp = base::Milliseconds(5)}));
  EXPECT_EQ(queue_.PopDueWord(now, /*rate=*/1.0), std::nullopt);
}

TEST_F(WordBoundaryQueueTest, TimeUntilNextWordDueIsCappedAtMaxExtrapolation) {
  const WordTiming word = MakeWord(/*start_time=*/kMaxExtrapolation * 3,
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/kMaxExtrapolation * 4);
  queue_.SetAudibleAnchor(/*media_time=*/base::TimeDelta(), anchor_wall_time_);

  EXPECT_EQ(queue_.TimeUntilNextWordDue(anchor_wall_time_, /*rate=*/1.0),
            kMaxExtrapolation);
}

TEST_F(WordBoundaryQueueTest, AnchorExactlyMaxExtrapolationOldIsNotStale) {
  // A pump that waited the capped TimeUntilNextWordDue() wakes up exactly
  // kMaxExtrapolation after the anchor, and must still be able to pop.
  const WordTiming word = MakeWord(/*start_time=*/kMaxExtrapolation,
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/kMaxExtrapolation * 2);
  queue_.SetAudibleAnchor(/*media_time=*/base::TimeDelta(), anchor_wall_time_);

  EXPECT_EQ(
      queue_.PopDueWord(anchor_wall_time_ + kMaxExtrapolation, /*rate=*/1.0),
      (DueWord{.timing = word, .audio_timestamp = kMaxExtrapolation}));
}

TEST_F(WordBoundaryQueueTest, StaleAnchorPopsNothing) {
  // An anchor older than kMaxExtrapolation means playback is paused or
  // stalled, so even a word that would be due by now is held back.
  const WordTiming word = MakeWord(/*start_time=*/base::TimeDelta(),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));
  queue_.SetAudibleAnchor(/*media_time=*/base::TimeDelta(), anchor_wall_time_);

  EXPECT_EQ(queue_.PopDueWord(
                anchor_wall_time_ + kMaxExtrapolation + base::Microseconds(1),
                /*rate=*/1.0),
            std::nullopt);
  EXPECT_FALSE(queue_.empty());
}

TEST_F(WordBoundaryQueueTest, StaleAnchorHasNothingToWaitFor) {
  const WordTiming word = MakeWord(/*start_time=*/kMaxExtrapolation * 2,
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/kMaxExtrapolation * 3);
  queue_.SetAudibleAnchor(/*media_time=*/base::TimeDelta(), anchor_wall_time_);

  EXPECT_EQ(queue_.TimeUntilNextWordDue(
                anchor_wall_time_ + kMaxExtrapolation + base::Microseconds(1),
                /*rate=*/1.0),
            std::nullopt);
}

TEST_F(WordBoundaryQueueTest, ZeroRatePopsNothing) {
  const WordTiming word = MakeWord(/*start_time=*/base::TimeDelta(),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));
  queue_.SetAudibleAnchor(/*media_time=*/base::TimeDelta(), anchor_wall_time_);

  EXPECT_EQ(queue_.PopDueWord(anchor_wall_time_, /*rate=*/0.0), std::nullopt);
}

TEST_F(WordBoundaryQueueTest, ZeroRateHasNothingToWaitFor) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(5),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));
  queue_.SetAudibleAnchor(/*media_time=*/base::TimeDelta(), anchor_wall_time_);

  EXPECT_EQ(queue_.TimeUntilNextWordDue(anchor_wall_time_, /*rate=*/0.0),
            std::nullopt);
}

TEST_F(WordBoundaryQueueTest, NegativeRatePopsNothing) {
  const WordTiming word = MakeWord(/*start_time=*/base::TimeDelta(),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));
  queue_.SetAudibleAnchor(/*media_time=*/base::TimeDelta(), anchor_wall_time_);

  EXPECT_EQ(queue_.PopDueWord(anchor_wall_time_, /*rate=*/-1.0), std::nullopt);
}

TEST_F(WordBoundaryQueueTest, NegativeRateHasNothingToWaitFor) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(5),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));
  queue_.SetAudibleAnchor(/*media_time=*/base::TimeDelta(), anchor_wall_time_);

  EXPECT_EQ(queue_.TimeUntilNextWordDue(anchor_wall_time_, /*rate=*/-1.0),
            std::nullopt);
}

TEST_F(WordBoundaryQueueTest, TimeUntilNextWordDueScalesWithPlaybackRate) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(40),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(50));
  queue_.SetAudibleAnchor(/*media_time=*/base::Milliseconds(20),
                          anchor_wall_time_);

  // 20 ms of media time past the anchor take 10 ms of wall time at 2x.
  EXPECT_EQ(queue_.TimeUntilNextWordDue(anchor_wall_time_, /*rate=*/2.0),
            base::Milliseconds(10));
}

TEST_F(WordBoundaryQueueTest, TimestampScalesWithPlaybackRate) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(40),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(50));
  queue_.SetAudibleAnchor(/*media_time=*/base::Milliseconds(20),
                          anchor_wall_time_);

  // 10 ms of wall time past the anchor play 20 ms of media time at 2x.
  EXPECT_EQ(
      queue_.PopDueWord(anchor_wall_time_ + base::Milliseconds(10),
                        /*rate=*/2.0),
      (DueWord{.timing = word, .audio_timestamp = base::Milliseconds(40)}));
}

TEST_F(WordBoundaryQueueTest, ClearDropsPendingWords) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(5),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/4);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));

  queue_.Clear();

  EXPECT_TRUE(queue_.empty());
}

TEST_F(WordBoundaryQueueTest, ClearInvalidatesAnchor) {
  queue_.SetAudibleAnchor(/*media_time=*/base::TimeDelta(), anchor_wall_time_);
  queue_.Clear();
  const WordTiming word = MakeWord(/*start_time=*/base::TimeDelta(),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));

  // No word pops until a new anchor is set after Clear().
  EXPECT_EQ(queue_.PopDueWord(anchor_wall_time_, /*rate=*/1.0), std::nullopt);
}

TEST_F(WordBoundaryQueueTest, ClearRestartsMediaTimeAtZero) {
  const WordTiming word_before_clear =
      MakeWord(/*start_time=*/base::Milliseconds(5),
               /*start_character_offset=*/0,
               /*end_character_offset=*/4);
  queue_.Enqueue(/*timings=*/{}, /*duration=*/base::Milliseconds(10));
  queue_.Enqueue(base::span_from_ref(word_before_clear),
                 /*duration=*/base::Milliseconds(10));
  queue_.Clear();
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(1),
                                   /*start_character_offset=*/8,
                                   /*end_character_offset=*/12);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));
  queue_.SetAudibleAnchor(/*media_time=*/base::TimeDelta(), anchor_wall_time_);

  // Not shifted by the 20 ms enqueued before Clear().
  EXPECT_EQ(
      queue_.PopDueWord(anchor_wall_time_ + base::Milliseconds(1),
                        /*rate=*/1.0),
      (DueWord{.timing = word, .audio_timestamp = base::Milliseconds(1)}));
}

}  // namespace

}  // namespace readaloud
