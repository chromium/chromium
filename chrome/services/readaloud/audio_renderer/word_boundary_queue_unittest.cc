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
#include "media/filters/audio_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

namespace {

constexpr base::TimeDelta kMaxExtrapolation =
    WordBoundaryQueue::kMaxExtrapolation;

// One frame per millisecond, so frame counts read as milliseconds.
constexpr int kSampleRate = 1000;

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
  // Writes `frames` frames (milliseconds) of audio at `rate` to the clock, as
  // one render with an output delay of `delay_frames`, and anchors the queue
  // at `anchor_wall_time_`.
  void WriteAudio(int frames, double rate = 1.0, int delay_frames = 0) {
    clock_.WroteAudio(frames, /*frames_requested=*/frames, delay_frames, rate);
    queue_.SetAnchorTime(anchor_wall_time_);
  }

  std::optional<WordTiming> PopDueWord(base::TimeTicks now, double rate = 1.0) {
    return queue_.PopDueWord(now, clock_, rate);
  }

  std::optional<base::TimeDelta> TimeUntilNextWordDue(base::TimeTicks now,
                                                      double rate = 1.0) {
    return queue_.TimeUntilNextWordDue(now, clock_, rate);
  }

  // Arbitrary wall time at which the tests anchor the queue.
  const base::TimeTicks anchor_wall_time_ =
      base::TimeTicks() + base::Seconds(100);
  media::AudioClock clock_{/*start_timestamp=*/base::TimeDelta(), kSampleRate};
  WordBoundaryQueue queue_;
};

TEST_F(WordBoundaryQueueTest, EmptyQueuePopsNothing) {
  WriteAudio(/*frames=*/10);

  EXPECT_EQ(PopDueWord(anchor_wall_time_), std::nullopt);
}

TEST_F(WordBoundaryQueueTest, EmptyQueueHasNothingToWaitFor) {
  WriteAudio(/*frames=*/10);

  EXPECT_EQ(TimeUntilNextWordDue(anchor_wall_time_), std::nullopt);
}

TEST_F(WordBoundaryQueueTest, PopsWordAudibleAtAnchor) {
  const WordTiming word = MakeWord(/*start_time=*/base::TimeDelta(),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));
  WriteAudio(/*frames=*/10);

  EXPECT_EQ(PopDueWord(anchor_wall_time_), word);
  EXPECT_TRUE(queue_.empty());
}

TEST_F(WordBoundaryQueueTest, PopsWordAlreadyPlayingAtAnchor) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(5),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(40));
  // The second render plays out the first 20 ms, past the word's start.
  WriteAudio(/*frames=*/20);
  WriteAudio(/*frames=*/20);
  ASSERT_EQ(clock_.front_timestamp(), base::Milliseconds(20));

  EXPECT_EQ(PopDueWord(anchor_wall_time_), word);
}

TEST_F(WordBoundaryQueueTest, DoesNotPopWordBeforeItIsAudible) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(30),
                                   /*start_character_offset=*/6,
                                   /*end_character_offset=*/11);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(40));
  // Media time 20 ms is audible at the anchor.
  WriteAudio(/*frames=*/20);
  WriteAudio(/*frames=*/20);

  // The word becomes audible 10 ms after the anchor.
  EXPECT_EQ(PopDueWord(anchor_wall_time_ + base::Milliseconds(10) -
                       base::Microseconds(1)),
            std::nullopt);
  EXPECT_FALSE(queue_.empty());
}

TEST_F(WordBoundaryQueueTest, TimeUntilNextWordDueIsRelativeToAnchor) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(30),
                                   /*start_character_offset=*/6,
                                   /*end_character_offset=*/11);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(40));
  WriteAudio(/*frames=*/20);
  WriteAudio(/*frames=*/20);

  EXPECT_EQ(TimeUntilNextWordDue(anchor_wall_time_), base::Milliseconds(10));
}

TEST_F(WordBoundaryQueueTest, PopsWordOnceAudibleRelativeToAnchor) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(30),
                                   /*start_character_offset=*/6,
                                   /*end_character_offset=*/11);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(40));
  WriteAudio(/*frames=*/20);
  WriteAudio(/*frames=*/20);

  EXPECT_EQ(PopDueWord(anchor_wall_time_ + base::Milliseconds(10)), word);
}

TEST_F(WordBoundaryQueueTest, TimeUntilNextWordDueIsZeroForOverdueWord) {
  const WordTiming word = MakeWord(/*start_time=*/base::TimeDelta(),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));
  WriteAudio(/*frames=*/10);

  EXPECT_EQ(TimeUntilNextWordDue(anchor_wall_time_ + base::Milliseconds(5)),
            base::TimeDelta());
}

TEST_F(WordBoundaryQueueTest, WaitsForOutputDelayBeforeFirstWord) {
  // The first write after a start or flush queues the output delay as
  // silence ahead of the audio.
  const WordTiming word = MakeWord(/*start_time=*/base::TimeDelta(),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));
  WriteAudio(/*frames=*/10, /*rate=*/1.0, /*delay_frames=*/50);

  EXPECT_EQ(TimeUntilNextWordDue(anchor_wall_time_), base::Milliseconds(50));
}

TEST_F(WordBoundaryQueueTest, DoesNotPopFirstWordDuringOutputDelay) {
  const WordTiming word = MakeWord(/*start_time=*/base::TimeDelta(),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));
  WriteAudio(/*frames=*/10, /*rate=*/1.0, /*delay_frames=*/50);

  EXPECT_EQ(PopDueWord(anchor_wall_time_ + base::Milliseconds(50) -
                       base::Microseconds(1)),
            std::nullopt);
}

TEST_F(WordBoundaryQueueTest, PopsFirstWordOnceOutputDelayHasPlayed) {
  const WordTiming word = MakeWord(/*start_time=*/base::TimeDelta(),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));
  WriteAudio(/*frames=*/10, /*rate=*/1.0, /*delay_frames=*/50);

  EXPECT_EQ(PopDueWord(anchor_wall_time_ + base::Milliseconds(50)), word);
}

TEST_F(WordBoundaryQueueTest, WordInUnwrittenAudioIsDueAfterBufferedAudio) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(15),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(20));
  // 50 ms of silence and 10 ms of audio are buffered; the word starts 5 ms
  // into the audio not written yet.
  WriteAudio(/*frames=*/10, /*rate=*/1.0, /*delay_frames=*/50);

  EXPECT_EQ(TimeUntilNextWordDue(anchor_wall_time_), base::Milliseconds(65));
}

TEST_F(WordBoundaryQueueTest, WordInUnwrittenAudioIsDueAtPlaybackRate) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(30),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(40));
  WriteAudio(/*frames=*/10, /*rate=*/1.0);

  // 10 ms of buffered audio, then 20 ms of media time at 2x take 10 ms.
  EXPECT_EQ(TimeUntilNextWordDue(anchor_wall_time_, /*rate=*/2.0),
            base::Milliseconds(20));
}

TEST_F(WordBoundaryQueueTest, WordInAudioBufferedAtOlderRateIsDueAtThatRate) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(5),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(40));
  // 10 ms of output at 1x, then 10 ms at 2x, both still buffered.
  WriteAudio(/*frames=*/10, /*rate=*/1.0);
  WriteAudio(/*frames=*/10, /*rate=*/2.0, /*delay_frames=*/10);

  // The word is in the 1x audio, so the current 2x rate doesn't apply.
  EXPECT_EQ(TimeUntilNextWordDue(anchor_wall_time_, /*rate=*/2.0),
            base::Milliseconds(5));
}

TEST_F(WordBoundaryQueueTest, WordInAudioBufferedAtNewerRateIsDueAtThatRate) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(20),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(40));
  WriteAudio(/*frames=*/10, /*rate=*/1.0);
  WriteAudio(/*frames=*/10, /*rate=*/2.0, /*delay_frames=*/10);

  // 10 ms of 1x audio, then 10 ms of media time at 2x take 5 ms.
  EXPECT_EQ(TimeUntilNextWordDue(anchor_wall_time_, /*rate=*/1.0),
            base::Milliseconds(15));
}

TEST_F(WordBoundaryQueueTest, ShiftsTimingsByDurationEnqueuedBefore) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(1),
                                   /*start_character_offset=*/7,
                                   /*end_character_offset=*/9);
  queue_.Enqueue(/*timings=*/{}, /*duration=*/base::Milliseconds(10));
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));
  WriteAudio(/*frames=*/20);

  // 1 ms into the second segment is 11 ms into the media timeline.
  EXPECT_EQ(TimeUntilNextWordDue(anchor_wall_time_), base::Milliseconds(11));
  EXPECT_EQ(PopDueWord(anchor_wall_time_ + base::Milliseconds(11)),
            Shifted(word, base::Milliseconds(10)));
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
  WriteAudio(/*frames=*/40);

  // 1 ms into the third segment is 31 ms into the media timeline.
  EXPECT_EQ(TimeUntilNextWordDue(anchor_wall_time_), base::Milliseconds(31));
  EXPECT_EQ(PopDueWord(anchor_wall_time_ + base::Milliseconds(31)),
            Shifted(word, base::Milliseconds(30)));
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
  WriteAudio(/*frames=*/10);
  const base::TimeTicks now = anchor_wall_time_ + base::Milliseconds(5);

  EXPECT_EQ(PopDueWord(now), words[0]);
  EXPECT_EQ(PopDueWord(now), words[1]);
  EXPECT_EQ(PopDueWord(now), std::nullopt);
}

TEST_F(WordBoundaryQueueTest, TimeUntilNextWordDueIsCappedAtMaxExtrapolation) {
  const WordTiming word = MakeWord(/*start_time=*/kMaxExtrapolation * 3,
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/kMaxExtrapolation * 4);
  WriteAudio(/*frames=*/10);

  EXPECT_EQ(TimeUntilNextWordDue(anchor_wall_time_), kMaxExtrapolation);
}

TEST_F(WordBoundaryQueueTest, AnchorExactlyMaxExtrapolationOldIsNotStale) {
  // A pump that waited the capped TimeUntilNextWordDue() wakes up exactly
  // kMaxExtrapolation after the anchor, and must still be able to pop.
  const WordTiming word = MakeWord(/*start_time=*/kMaxExtrapolation,
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/kMaxExtrapolation * 2);
  WriteAudio(/*frames=*/10);

  EXPECT_EQ(PopDueWord(anchor_wall_time_ + kMaxExtrapolation), word);
}

TEST_F(WordBoundaryQueueTest, StaleAnchorPopsNothing) {
  // An anchor older than kMaxExtrapolation means playback is paused or
  // stalled, so even a word that would be due by now is held back.
  const WordTiming word = MakeWord(/*start_time=*/base::TimeDelta(),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));
  WriteAudio(/*frames=*/10);

  EXPECT_EQ(
      PopDueWord(anchor_wall_time_ + kMaxExtrapolation + base::Microseconds(1)),
      std::nullopt);
  EXPECT_FALSE(queue_.empty());
}

TEST_F(WordBoundaryQueueTest, StaleAnchorHasNothingToWaitFor) {
  const WordTiming word = MakeWord(/*start_time=*/kMaxExtrapolation * 2,
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/kMaxExtrapolation * 3);
  WriteAudio(/*frames=*/10);

  EXPECT_EQ(TimeUntilNextWordDue(anchor_wall_time_ + kMaxExtrapolation +
                                 base::Microseconds(1)),
            std::nullopt);
}

TEST_F(WordBoundaryQueueTest, ZeroRateStillPopsWordInBufferedAudio) {
  // Audio already buffered plays out at the rate it was written at.
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(5),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));
  WriteAudio(/*frames=*/10);

  EXPECT_EQ(PopDueWord(anchor_wall_time_ + base::Milliseconds(5),
                       /*rate=*/0.0),
            word);
}

TEST_F(WordBoundaryQueueTest, ZeroRateHasNothingToWaitForInUnwrittenAudio) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(15),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(20));
  WriteAudio(/*frames=*/10);

  EXPECT_EQ(TimeUntilNextWordDue(anchor_wall_time_, /*rate=*/0.0),
            std::nullopt);
}

TEST_F(WordBoundaryQueueTest, NegativeRateHasNothingToWaitForInUnwrittenAudio) {
  const WordTiming word = MakeWord(/*start_time=*/base::Milliseconds(15),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(20));
  WriteAudio(/*frames=*/10);

  EXPECT_EQ(TimeUntilNextWordDue(anchor_wall_time_, /*rate=*/-1.0),
            std::nullopt);
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
  WriteAudio(/*frames=*/10);
  queue_.Clear();
  const WordTiming word = MakeWord(/*start_time=*/base::TimeDelta(),
                                   /*start_character_offset=*/0,
                                   /*end_character_offset=*/5);
  queue_.Enqueue(base::span_from_ref(word),
                 /*duration=*/base::Milliseconds(10));

  // No word pops until a new anchor is set after Clear().
  EXPECT_EQ(PopDueWord(anchor_wall_time_), std::nullopt);
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
  WriteAudio(/*frames=*/10);

  // Not shifted by the 20 ms enqueued before Clear().
  EXPECT_EQ(PopDueWord(anchor_wall_time_ + base::Milliseconds(1)), word);
}

}  // namespace

}  // namespace readaloud
