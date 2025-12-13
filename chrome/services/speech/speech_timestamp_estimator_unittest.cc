// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/speech_timestamp_estimator.h"

#include "base/time/time.h"
#include "base/types/zip.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace speech {
namespace {

using SpeechTimestamp = SpeechTimestampEstimator::SpeechTimestamp;
using PlaybackDuration = SpeechTimestampEstimator::PlaybackDuration;
using MediaTimestamp = SpeechTimestampEstimator::MediaTimestamp;
using MediaTimestampRange = media::MediaTimestampRange;
using MediaRanges = SpeechTimestampEstimator::MediaRanges;

using SpeechTimestampRange = std::pair<SpeechTimestamp, SpeechTimestamp>;

SpeechTimestampRange SpeechSecondsRange(int start, int end) {
  return {SpeechTimestamp(base::Seconds(start)),
          SpeechTimestamp(base::Seconds(end))};
}

MediaTimestampRange MediaSecondsRange(int start, int end) {
  return {.start = base::Seconds(start), .end = base::Seconds(end)};
}

void VerifyRanges(const MediaRanges& actual_ranges,
                  const MediaRanges& expected_ranges) {
  EXPECT_EQ(actual_ranges.size(), expected_ranges.size());
  for (auto [actual, expected] : base::zip(actual_ranges, expected_ranges)) {
    EXPECT_EQ(actual.start, expected.start);
    EXPECT_EQ(actual.end, expected.end);

    EXPECT_LT(actual.start, actual.end);
  }
}

class SpeechTimestampEstimatorTest : public testing::Test {
 public:
  SpeechTimestampEstimatorTest() = default;
  ~SpeechTimestampEstimatorTest() override = default;

  // Helper functions for taking the timestamps.
  MediaRanges TakeRange(SpeechTimestampRange range) {
    return estimator_.TakeTimestampsInRange(range.first, range.second);
  }
  MediaRanges TakeAllRanges() {
    // Completely empties out `estimator_`.
    return TakeRange(SpeechSecondsRange(0, base::Days(1).InSeconds()));
  }
  MediaRanges PeekRange(SpeechTimestampRange range) {
    return estimator_.PeekTimestampsInRange(range.first, range.second);
  }

  void AppendDuration(base::TimeDelta duration) {
    estimator_.AppendDuration(PlaybackDuration(duration));
  }

  void SkipSilence(base::TimeDelta duration) {
    estimator_.OnSilentMediaDropped(PlaybackDuration(duration));
  }

  void AddNewPlayback(base::TimeDelta start) {
    estimator_.AddPlaybackStart(MediaTimestamp(start));
  }

 private:
  SpeechTimestampEstimator estimator_;
};

TEST_F(SpeechTimestampEstimatorTest, NoAudio) {
  auto results = TakeAllRanges();
  EXPECT_TRUE(results.empty());
}

TEST_F(SpeechTimestampEstimatorTest, NoPlayback) {
  AppendDuration(base::Seconds(10));
  auto results = TakeAllRanges();
  EXPECT_TRUE(results.empty());
}

TEST_F(SpeechTimestampEstimatorTest, SinglePlayback_Basic) {
  constexpr base::TimeDelta kPlaybackDuration = base::Seconds(10);
  {
    SCOPED_TRACE("[0s,10s)");
    AddNewPlayback(base::Seconds(0));
    AppendDuration(kPlaybackDuration);
    VerifyRanges(TakeAllRanges(), {MediaSecondsRange(0, 10)});
  }

  {
    SCOPED_TRACE("[100s,110s)");
    AddNewPlayback(base::Seconds(100));
    AppendDuration(kPlaybackDuration);
    VerifyRanges(TakeAllRanges(), {MediaSecondsRange(100, 110)});
  }

  {
    SCOPED_TRACE("[-1s,9s)");
    AddNewPlayback(base::Seconds(-1));
    AppendDuration(kPlaybackDuration);
    VerifyRanges(TakeAllRanges(), {MediaSecondsRange(-1, 9)});
  }

  {
    SCOPED_TRACE("[1s,11s)");
    AddNewPlayback(base::Seconds(1));
    AppendDuration(kPlaybackDuration);
    VerifyRanges(TakeAllRanges(), {MediaSecondsRange(1, 11)});
  }
}

TEST_F(SpeechTimestampEstimatorTest, SinglePlayback_MultiplePlaybackChunks) {
  {
    SCOPED_TRACE("[0s,10s) - two chunks");
    AddNewPlayback(base::Seconds(0));
    AppendDuration(base::Seconds(5));
    AppendDuration(base::Seconds(5));
    VerifyRanges(TakeAllRanges(), {MediaSecondsRange(0, 10)});
  }

  {
    SCOPED_TRACE("[0s,10s) - ten chunks");
    AddNewPlayback(base::Seconds(0));
    for (int i = 0; i < 10; ++i) {
      AppendDuration(base::Seconds(1));
    }
    VerifyRanges(TakeAllRanges(), {MediaSecondsRange(0, 10)});
  }
}

TEST_F(SpeechTimestampEstimatorTest, SinglePlayback_OffsetStart) {
  // "Play" 5s of audio, without playback info.
  AppendDuration(base::Seconds(5));

  // Add [100, 110s).
  AddNewPlayback(base::Seconds(100));
  AppendDuration(base::Seconds(10));

  // Take the first 10s of playback. Only 5 of those seconds contain playback
  // info.
  VerifyRanges(TakeRange(SpeechSecondsRange(0, 10)),
               {MediaSecondsRange(100, 105)});
}

TEST_F(SpeechTimestampEstimatorTest, SinglePlayback_RangeLimits) {
  constexpr int kPlaybackSeconds = 10;

  // Add [100, 110s).
  AddNewPlayback(base::Seconds(100));
  AppendDuration(base::Seconds(kPlaybackSeconds));

  // Only [0, kPlaybackSeconds) makes sense technically, so make sure ranges
  // that are completely outside of those bounds are ignored.
  EXPECT_TRUE(TakeRange(SpeechSecondsRange(-100, -50)).empty());
  EXPECT_TRUE(TakeRange(SpeechSecondsRange(1000, 2000)).empty());

  // Make sure ranges right at the boundary are ignored.
  EXPECT_TRUE(
      TakeRange(SpeechSecondsRange(kPlaybackSeconds, 2 * kPlaybackSeconds))
          .empty());

  // Ranges which include [0,kPlaybackSeconds) are allowed.
  VerifyRanges(
      TakeRange(SpeechSecondsRange(-kPlaybackSeconds, 10 * kPlaybackSeconds)),
      {MediaSecondsRange(100, 110)});
}

TEST_F(SpeechTimestampEstimatorTest, SinglePlayback_AppendAfterDrainingFifo) {
  constexpr base::TimeDelta kPlaybackDuration = base::Seconds(10);

  // Add [100, 110s).
  AddNewPlayback(base::Seconds(100));
  AppendDuration(kPlaybackDuration);

  // Take all of the current range. Use a huge upper bound for good measure.
  std::ignore = TakeRange(SpeechSecondsRange(0, base::Days(25).InSeconds()));

  // Add another [110, 120s), without a AddNewPlayback() call.
  AppendDuration(kPlaybackDuration);
  VerifyRanges(TakeRange(SpeechSecondsRange(10, 20)),
               {MediaSecondsRange(110, 120)});
}

TEST_F(SpeechTimestampEstimatorTest, SinglePlayback_PartialRanges) {
  // Add [100, 110s).
  AddNewPlayback(base::Seconds(100));
  AppendDuration(base::Seconds(10));

  VerifyRanges(TakeRange(SpeechSecondsRange(0, 5)),
               {MediaSecondsRange(100, 105)});

  // Getting the same range twice should yield empty results
  EXPECT_TRUE(TakeRange(SpeechSecondsRange(0, 5)).empty());

  VerifyRanges(TakeRange(SpeechSecondsRange(5, 10)),
               {MediaSecondsRange(105, 110)});
}

TEST_F(SpeechTimestampEstimatorTest,
       SinglePlayback_LaterRangesDiscardPreviousRanges) {
  // Add [100, 110s).
  AddNewPlayback(base::Seconds(100));
  AppendDuration(base::Seconds(10));

  // Add [200, 210s).
  AddNewPlayback(base::Seconds(200));
  AppendDuration(base::Seconds(10));

  VerifyRanges(TakeRange(SpeechSecondsRange(15, 20)),
               {MediaSecondsRange(205, 210)});

  // All ranges before `base::Seconds(15)` should be discarded.
  EXPECT_TRUE(TakeRange(SpeechSecondsRange(0, 10)).empty());
  EXPECT_TRUE(TakeRange(SpeechSecondsRange(10, 15)).empty());
}

TEST_F(SpeechTimestampEstimatorTest, MultiplePlaybacks_Simple) {
  // Add [100s, 110s).
  AddNewPlayback(base::Seconds(100));
  AppendDuration(base::Seconds(10));

  // Add [120s, 140s).
  AddNewPlayback(base::Seconds(120));
  AppendDuration(base::Seconds(20));

  // Add [95s, 105s).
  AddNewPlayback(base::Seconds(95));
  AppendDuration(base::Seconds(10));

  // Add [105s, 115s).
  AddNewPlayback(base::Seconds(105));
  AppendDuration(base::Seconds(10));

  // Expect [100s, 110s), [120s, 140s), [95s, 105s), [105s, 115s).
  VerifyRanges(TakeAllRanges(),
               {MediaSecondsRange(100, 110), MediaSecondsRange(120, 140),
                MediaSecondsRange(95, 105), MediaSecondsRange(105, 115)});
}

TEST_F(SpeechTimestampEstimatorTest, MultiplePlaybacks_EmptyPlaybacks) {
  // Add a playback without duration.
  AddNewPlayback(base::Seconds(60));

  // Add [100s, 110s).
  AddNewPlayback(base::Seconds(100));
  AppendDuration(base::Seconds(10));

  // Add more playbacks without duration.
  AddNewPlayback(base::Seconds(110));
  AddNewPlayback(base::Seconds(50));
  AddNewPlayback(base::Seconds(50));

  // Add [200s, 210s).
  AddNewPlayback(base::Seconds(200));
  AppendDuration(base::Seconds(10));

  // Add more playbacks without duration.
  AddNewPlayback(base::Seconds(300));

  // Expect [100s, 110s), [200s, 210s).
  VerifyRanges(TakeAllRanges(),
               {MediaSecondsRange(100, 110), MediaSecondsRange(200, 210)});

  // This should add [300s, 310s).
  AppendDuration(base::Seconds(10));

  // Expect [300s, 310s).
  VerifyRanges(TakeAllRanges(), {MediaSecondsRange(300, 310)});
}

TEST_F(SpeechTimestampEstimatorTest, MultiplePlaybacks_PartialRanges) {
  // Add [100s, 110s).
  AddNewPlayback(base::Seconds(100));
  AppendDuration(base::Seconds(10));

  // Add [140s, 145s).
  AddNewPlayback(base::Seconds(140));
  AppendDuration(base::Seconds(5));

  // Add [100s, 110s).
  AddNewPlayback(base::Seconds(100));
  AppendDuration(base::Seconds(10));

  // Take 20s of playback, starting at 1s.
  // Expect [101s, 110s), [140s, 145s), [100s, 106s).
  VerifyRanges(TakeRange(SpeechSecondsRange(1, 21)),
               {MediaSecondsRange(101, 110), MediaSecondsRange(140, 145),
                MediaSecondsRange(100, 106)});

  // Take the last second of playback.
  // Expect [109s, 110s).
  VerifyRanges(TakeRange(SpeechSecondsRange(24, 25)),
               {MediaSecondsRange(109, 110)});
}

TEST_F(SpeechTimestampEstimatorTest, Silences_Simple) {
  // Add [100s, 110s).
  AddNewPlayback(base::Seconds(100));
  AppendDuration(base::Seconds(10));

  // Media time should increase to 120s, speech time should not.
  SkipSilence(base::Seconds(10));

  // Should result in [120,130s) being added
  AppendDuration(base::Seconds(10));

  // Take 20s of playback. Silences shouldn't be included.
  VerifyRanges(TakeRange(SpeechSecondsRange(0, 20)),
               {MediaSecondsRange(100, 110), MediaSecondsRange(120, 130)});
}

TEST_F(SpeechTimestampEstimatorTest, Silences_MultipleChunks) {
  // Add [100s, 110s).
  AddNewPlayback(base::Seconds(100));
  AppendDuration(base::Seconds(10));

  // Media time should increase to 120s, speech time should not.
  SkipSilence(base::Seconds(5));
  SkipSilence(base::Seconds(5));

  // Should result in [120,130s) being added
  AppendDuration(base::Seconds(10));

  // Take 20s of playback. Silences shouldn't be included.
  VerifyRanges(TakeRange(SpeechSecondsRange(0, 20)),
               {MediaSecondsRange(100, 110), MediaSecondsRange(120, 130)});
}

TEST_F(SpeechTimestampEstimatorTest, Silences_EndSilence) {
  // Add [100s, 110s).
  AddNewPlayback(base::Seconds(100));
  AppendDuration(base::Seconds(10));

  SkipSilence(base::Seconds(10));

  // Trailing silence should be ignored.
  VerifyRanges(TakeAllRanges(), {MediaSecondsRange(100, 110)});
}

TEST_F(SpeechTimestampEstimatorTest, Silences_StartSilence) {
  SkipSilence(base::Seconds(10));

  // Add [100s, 110s).
  AddNewPlayback(base::Seconds(100));
  AppendDuration(base::Seconds(10));

  // Silences before playback start should be ignored.
  VerifyRanges(TakeAllRanges(), {MediaSecondsRange(100, 110)});
}

TEST_F(SpeechTimestampEstimatorTest, Silences_SilenceAndDurationBeforeStart) {
  SkipSilence(base::Seconds(10));
  AppendDuration(base::Seconds(10));
  SkipSilence(base::Seconds(10));

  // Add [100s, 110s).
  AddNewPlayback(base::Seconds(100));
  AppendDuration(base::Seconds(10));

  // Only 10s of speech time should have no media associated with it.
  VerifyRanges(TakeRange(SpeechSecondsRange(0, 10)), {});
  VerifyRanges(TakeRange(SpeechSecondsRange(10, 20)),
               {MediaSecondsRange(100, 110)});
}

TEST_F(SpeechTimestampEstimatorTest, Silences_TakeDuringSilence) {
  // Add [100s, 110s).
  AddNewPlayback(base::Seconds(100));
  AppendDuration(base::Seconds(10));

  // Media time should be 120s.
  SkipSilence(base::Seconds(10));

  // Take for more than the duration of the first playback. This should only
  // return the first playback, since the silence is not included.
  VerifyRanges(TakeRange(SpeechSecondsRange(0, 20)),
               {MediaSecondsRange(100, 110)});

  // Media time should be 130s.
  SkipSilence(base::Seconds(10));

  // Should add [130, 140s)
  AppendDuration(base::Seconds(10));

  // Media time should have kept increasing with the silence, regardless of us
  // taking ranges.
  VerifyRanges(TakeRange(SpeechSecondsRange(10, 20)),
               {MediaSecondsRange(130, 140)});
}

TEST_F(SpeechTimestampEstimatorTest, Silences_PlaybackResetsSilence_Simple) {
  // Add [100s, 110s).
  AddNewPlayback(base::Seconds(100));
  AppendDuration(base::Seconds(10));

  // Media time should be 120s.
  SkipSilence(base::Seconds(10));

  // Add [200s, 210s). Should discard the silence above.
  AddNewPlayback(base::Seconds(200));
  AppendDuration(base::Seconds(10));

  // Take all all ranges. The second playback should have discarded the silence.
  VerifyRanges(TakeRange(SpeechSecondsRange(0, 30)),
               {MediaSecondsRange(100, 110), MediaSecondsRange(200, 210)});
}

TEST_F(SpeechTimestampEstimatorTest,
       Silences_PlaybackResetsSilence_MultiplePlaybacks) {
  // Add [100s, 110s).
  AddNewPlayback(base::Seconds(100));
  AppendDuration(base::Seconds(10));

  // Media time should be 120s.
  SkipSilence(base::Seconds(10));

  AddNewPlayback(base::Seconds(200));

  // Media time should be 210s.
  SkipSilence(base::Seconds(10));

  // Add many media start times during the silences
  AddNewPlayback(base::Seconds(300));
  AddNewPlayback(base::Seconds(400));
  AddNewPlayback(base::Seconds(500));

  // Media time should be 510s.
  SkipSilence(base::Seconds(10));

  // Add [510s, 520s)
  AppendDuration(base::Seconds(10));

  VerifyRanges(TakeRange(SpeechSecondsRange(0, 20)),
               {MediaSecondsRange(100, 110), MediaSecondsRange(510, 520)});
}

TEST_F(SpeechTimestampEstimatorTest, PeekTimestampsInRange) {
  // Add [100s, 110s). Speech time: [0, 10)
  AddNewPlayback(base::Seconds(100));
  AppendDuration(base::Seconds(10));

  // Add [140s, 145s). Speech time: [10, 15)
  AddNewPlayback(base::Seconds(140));
  AppendDuration(base::Seconds(5));

  // Add [100s, 110s). Speech time: [15, 25)
  AddNewPlayback(base::Seconds(100));
  AppendDuration(base::Seconds(10));

  // 1. Peek into a range.
  // Speech range [1, 21) should correspond to media ranges:
  // [101, 110), [140, 145), [100, 106)
  MediaRanges expected_ranges = {MediaSecondsRange(101, 110),
                                 MediaSecondsRange(140, 145),
                                 MediaSecondsRange(100, 106)};
  VerifyRanges(PeekRange(SpeechSecondsRange(1, 21)), expected_ranges);

  // 2. Verify that peeking again yields the exact same result.
  // This is the key difference from TakeRange.
  VerifyRanges(PeekRange(SpeechSecondsRange(1, 21)), expected_ranges);

  // 3. Take the same range to prove the state was indeed unchanged.
  // The result of taking should be identical to the result of peeking.
  VerifyRanges(TakeRange(SpeechSecondsRange(1, 21)), expected_ranges);

  // 4. Now that we've *taken* the range [1, 21), the estimator is modified.
  // Peeking into the full range should now only return the final chunk.
  // Speech range [21, 25) -> Media range [106, 110)
  VerifyRanges(PeekRange(SpeechSecondsRange(0, 100)),
               {MediaSecondsRange(106, 110)});
}

}  // namespace
}  // namespace speech
