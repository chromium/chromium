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

namespace readaloud {

// Holds the word timings of the audio handed to the renderer that have not
// been dispatched yet, and predicts the wall clock time at which each word
// starts playing, so it can be highlighted in sync with the audio.
//
// Timings are stored in media time: plain offsets into the audio enqueued
// since the last Clear() rather than offsets into their own segment. An anchor
// pairs one media time with the wall clock time at which the audio at that
// media time is played, so PopDueWord() can extrapolate when later words will
// be played.
//
// Not thread-safe: callers must serialize access.
class WordBoundaryQueue {
 public:
  // The longest PopDueWord() extrapolates from one anchor. It bounds both how
  // long a caller is told to wait and how old an anchor is trusted.
  static constexpr base::TimeDelta kMaxExtrapolation = base::Milliseconds(200);

  // A word that has started playing. `audio_timestamp` is the media time
  // being played at the `now` passed to PopDueWord().
  struct DueWord {
    WordTiming timing;
    base::TimeDelta audio_timestamp;

    friend bool operator==(const DueWord&, const DueWord&) = default;
  };

  WordBoundaryQueue();

  WordBoundaryQueue(const WordBoundaryQueue&) = delete;
  WordBoundaryQueue& operator=(const WordBoundaryQueue&) = delete;

  ~WordBoundaryQueue();

  // Appends the timings of a segment whose audio lasts `duration`. `timings`
  // are relative to the start of that segment.
  void Enqueue(base::span<const WordTiming> timings, base::TimeDelta duration);

  // Records that the audio at `media_time` is played at `wall_time`.
  void SetAudibleAnchor(base::TimeDelta media_time, base::TimeTicks wall_time);

  // Pops the front word if it has started playing by `now`, assuming media
  // time advances at `rate` times wall time. A word is never popped before it
  // plays, but it may be popped late. Returns nullopt if no word is due.
  std::optional<DueWord> PopDueWord(base::TimeTicks now, double rate);

  // Returns how long after `now` the front word starts playing, capped at
  // kMaxExtrapolation. Returns nullopt if there is nothing to wait for.
  std::optional<base::TimeDelta> TimeUntilNextWordDue(base::TimeTicks now,
                                                      double rate) const;

  // Drops every word and restarts media time at zero.
  void Clear();

  bool empty() const { return pending_words_.empty(); }

 private:
  // Returns the wall time at which the front word starts playing, or nullopt
  // under the same conditions as TimeUntilNextWordDue().
  std::optional<base::TimeTicks> FrontWordDueTime(base::TimeTicks now,
                                                  double rate) const;

  // Words not dispatched yet, in order, in media time.
  base::circular_deque<WordTiming> pending_words_;

  // Duration of the audio enqueued since construction or the last Clear().
  // Shifts the next segment's timings into media time.
  base::TimeDelta enqueued_duration_;

  // The audio at `anchor_media_time_` is played at `anchor_wall_time_`.
  base::TimeDelta anchor_media_time_;
  base::TimeTicks anchor_wall_time_;
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_AUDIO_RENDERER_WORD_BOUNDARY_QUEUE_H_
