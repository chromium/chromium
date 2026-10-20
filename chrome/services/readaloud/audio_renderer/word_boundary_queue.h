// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_AUDIO_RENDERER_WORD_BOUNDARY_QUEUE_H_
#define CHROME_SERVICES_READALOUD_AUDIO_RENDERER_WORD_BOUNDARY_QUEUE_H_

#include <optional>

#include "base/containers/circular_deque.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "chrome/services/readaloud/word_timing.h"

namespace media {
class AudioClock;
}  // namespace media

namespace readaloud {

// Holds word timings for audio handed to the renderer and predicts the wall
// clock time when each word starts playing, keeping UI highlights in sync.
//
// Timings are tracked in media time (continuous offset since the last
// `Clear()`). Callers pair this with a `media::AudioClock` and anchor wall time
// (via `SetAnchorTime()`). The queue maps words to wall time by:
// 1. Returning due immediately if audio already reached the speakers
//    (start < front).
// 2. Querying `AudioClock::TimeUntilPlayback()` for buffered audio
//    (front <= start <= back), accounting for silence padding and variable
//    playback rates.
// 3. Extrapolating forward at `rate` for audio not yet buffered
//    (start > back).
//
// Not thread-safe: callers must serialize access.
class WordBoundaryQueue {
 public:
  // The longest `PopDueWord()` trusts one anchor. It bounds both how long a
  // caller is told to wait and how old an anchor is trusted.
  static constexpr base::TimeDelta kMaxExtrapolation = base::Milliseconds(200);

  WordBoundaryQueue();

  WordBoundaryQueue(const WordBoundaryQueue&) = delete;
  WordBoundaryQueue& operator=(const WordBoundaryQueue&) = delete;

  ~WordBoundaryQueue();

  // Appends the timings of a segment whose audio lasts `duration`. `timings`
  // are relative to the start of that segment.
  void Enqueue(base::span<const WordTiming> timings, base::TimeDelta duration);

  // Records that the audio clock passed to `PopDueWord()` and
  // `TimeUntilNextWordDue()` was last updated at `wall_time`: its front
  // timestamp is played at `wall_time`.
  void SetAnchorTime(base::TimeTicks wall_time);

  // Pops the front word if it has started playing by `wall_now`. `clock`
  // describes the audio buffered for output as of the anchor time; audio not
  // written to it yet is assumed to play right after it, at `rate`. A word is
  // never popped before it plays, but it may be popped late. Returns nullopt if
  // no word is due.
  std::optional<WordTiming> PopDueWord(base::TimeTicks wall_now,
                                       const media::AudioClock& clock,
                                       double rate);

  // Returns how long after `wall_now` the front word starts playing, capped at
  // kMaxExtrapolation. Returns nullopt if there is nothing to wait for.
  std::optional<base::TimeDelta> TimeUntilNextWordDue(
      base::TimeTicks wall_now,
      const media::AudioClock& clock,
      double rate) const;

  // Drops every word and restarts media time at zero.
  void Clear();

  bool empty() const { return pending_words_.empty(); }

 private:
  // Returns the wall time at which the front word starts playing, or nullopt
  // under the same conditions as `TimeUntilNextWordDue()`.
  std::optional<base::TimeTicks> FrontWordDueTime(
      base::TimeTicks wall_now,
      const media::AudioClock& clock,
      double rate) const;

  // Words not dispatched yet, in order, in media time.
  base::circular_deque<WordTiming> pending_words_;

  // Duration of the audio enqueued since construction or the last `Clear()`.
  // Shifts the next segment's timings into media time.
  base::TimeDelta enqueued_duration_;

  // The wall time at which the audio clock was last updated.
  base::TimeTicks anchor_wall_time_;
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_AUDIO_RENDERER_WORD_BOUNDARY_QUEUE_H_
