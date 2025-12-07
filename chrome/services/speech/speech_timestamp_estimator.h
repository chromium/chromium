// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_SPEECH_TIMESTAMP_ESTIMATOR_H_
#define CHROME_SERVICES_SPEECH_SPEECH_TIMESTAMP_ESTIMATOR_H_

#include <optional>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace speech {

// Helper class to match speech service transcriptions with the timestamps of
// the audio that originated the transcriptions. It does so by saving start
// timestamps in a FIFO and tracking how much audio has been sent to the speech
// service. Once a transcription is finalized, this class will retrieve the
// original timestamps based on "when" -- in audio time, not wall time -- the
// audio was sent to the speech service.
//
// This class deals with three types of `base::TimeDelta`, strongly typed as
// aliases to help avoid confusion:
// `MediaTimestamp`  : the originating media's timestamps. Media time can be
//                     seeked, but its playback rate is always 1.0, since the
//                     speech service receives audio before rate adjustments.
// `PlaybackDuration`: duration of the media sent to the speech service.
// `SpeechTimestamp` : starts at 0s and monotonically increases by the
//                     `PlaybackDuration` of every audio buffer sent to the
//                     speech service.
class SpeechTimestampEstimator {
 public:
  // See class header for descriptions.
  using SpeechTimestamp =
      base::StrongAlias<class SpeechTimestampTag, base::TimeDelta>;
  using MediaTimestamp =
      base::StrongAlias<class AudioTimestampTag, base::TimeDelta>;
  using PlaybackDuration =
      base::StrongAlias<class PlaybackDurationTag, base::TimeDelta>;

  // Note: these `base::TimeDelta`s are `MediaTimestamp`s, but avoid forcing
  // callers to deal with the `base::StrongAlias`.
  using MediaRanges = std::vector<media::MediaTimestampRange>;

  // Represents a chunk of originating media played for a certain amount of
  // time, and its position in the queue of all audio sent to the speech
  // service.
  struct PlaybackChunk {
    PlaybackChunk(MediaTimestamp media_start,
                  SpeechTimestamp current_speech_time);

    // Moves `media_start` and `speech_start` forward, decreasing
    // `speech_duration` accordingly.
    void TrimStart(PlaybackDuration duration);

    // Increases `playback_duration`.
    void AddDuration(PlaybackDuration duration);

    MediaTimestamp MediaEnd() const;
    SpeechTimestamp SpeechEnd() const;

    MediaTimestamp media_start;
    SpeechTimestamp speech_start;
    PlaybackDuration playback_duration;
  };

  SpeechTimestampEstimator();
  ~SpeechTimestampEstimator();

  // Marks the start of a new playback. Called when a file is first played, or
  // when the originating media is seeked.
  void AddPlaybackStart(MediaTimestamp media_start_pts);

  // Increases `current_speech_time_`, and the duration of the current playback.
  // Called anytime audio is sent to the speech service.
  void AppendDuration(PlaybackDuration duration);

  // Called each time we drop silent audio buffers, after detecting long
  // stretches of silence. Not sending silence to the speech service "pauses"
  // `current_speech_time_`, while the media time keeps increasing. This method
  // tracks total silence so we can adjust media timestamps accordingly on the
  // next `AppendDuration()` calls.
  void OnSilentMediaDropped(PlaybackDuration duration);

  // Given the [`start`,`end`) speech timestamps from finalized audio
  // transcriptions, returns the ranges of originating media timestamps.
  // Multiple ranges might be included between `start` and `end` (due to
  // seeks or silences), and returned ranges might overlap.
  // Note: discards all data prior to `start`. Does nothing if [`start`, `end`)
  //       doesn't overlap [0,`current_speech_time`).
  [[nodiscard]]
  MediaRanges TakeTimestampsInRange(SpeechTimestamp start, SpeechTimestamp end);

  // Similar to `TakeTimestampsInRange`, but this method will only peek at the
  // current timestamps, and will not discard any data.
  [[nodiscard]]
  MediaRanges PeekTimestampsInRange(SpeechTimestamp start, SpeechTimestamp end);

 private:
  // Given a vector of `PlaybackChunk`s, returns a `MediaRanges` vector with the
  // media start and end presentation timestamps.
  MediaRanges ConvertToMediaRanges(const std::vector<PlaybackChunk>& playbacks);

  // Moves forward the last media timestamp by `running_silence_duration_`.
  void AdjustLastMediaTimestampForSilence(SpeechTimestamp current_speech_time);

  // Removes all chunks in the given `deque` that are in the [0,
  // `end_timestamp`) range, splitting partial chunks if necessary.
  void PopFrontUntil(base::circular_deque<PlaybackChunk>& chunks,
                     SpeechTimestamp end_timestamp);

  // Returns all chunks in the given `deque` that are in the [0,
  // `end_timestamp`) range, splitting partial chunks if necessary.
  std::vector<PlaybackChunk> TakeFrontUntil(
      base::circular_deque<PlaybackChunk>& chunks,
      SpeechTimestamp end_timestamp);

  // Tracks how much silence was skipped since the last call to
  // AddPlaybackStart() or AddDuration().
  std::optional<PlaybackDuration> running_silence_duration_;

  // The running length of all audio sent to the speech service so far.
  SpeechTimestamp current_speech_time_;

  // Timeline representing what media was sent to the speech service, in order.
  base::circular_deque<PlaybackChunk> playback_chunks_;
};

}  // namespace speech

#endif  // CHROME_SERVICES_SPEECH_SPEECH_TIMESTAMP_ESTIMATOR_H_
