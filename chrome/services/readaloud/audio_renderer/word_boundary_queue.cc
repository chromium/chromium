// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/audio_renderer/word_boundary_queue.h"

#include <algorithm>

#include "media/filters/audio_clock.h"

namespace readaloud {

WordBoundaryQueue::WordBoundaryQueue() = default;

WordBoundaryQueue::~WordBoundaryQueue() = default;

void WordBoundaryQueue::Enqueue(base::span<const WordTiming> timings,
                                base::TimeDelta duration) {
  for (WordTiming timing : timings) {
    timing.start_time += enqueued_duration_;
    timing.end_time += enqueued_duration_;
    pending_words_.push_back(timing);
  }
  enqueued_duration_ += duration;
}

void WordBoundaryQueue::SetAnchorTime(base::TimeTicks wall_time) {
  anchor_wall_time_ = wall_time;
}

std::optional<WordTiming> WordBoundaryQueue::PopDueWord(
    base::TimeTicks wall_now,
    const media::AudioClock& clock,
    double rate) {
  const std::optional<base::TimeTicks> due_time =
      FrontWordDueTime(wall_now, clock, rate);
  if (!due_time || *due_time > wall_now) {
    return std::nullopt;
  }
  const WordTiming word = pending_words_.front();
  pending_words_.pop_front();
  // TODO(b/524284442): Log the average delay between highlighting a word and
  // its audio becoming audible.
  return word;
}

std::optional<base::TimeDelta> WordBoundaryQueue::TimeUntilNextWordDue(
    base::TimeTicks wall_now,
    const media::AudioClock& clock,
    double rate) const {
  const std::optional<base::TimeTicks> due_time =
      FrontWordDueTime(wall_now, clock, rate);
  if (!due_time) {
    return std::nullopt;
  }

  return std::clamp(*due_time - wall_now, base::TimeDelta(), kMaxExtrapolation);
}

std::optional<base::TimeTicks> WordBoundaryQueue::FrontWordDueTime(
    base::TimeTicks wall_now,
    const media::AudioClock& clock,
    double rate) const {
  if (pending_words_.empty()) {
    return std::nullopt;
  }

  // Staleness guard. An old anchor means playback is paused, stalled or
  // finished.
  if (wall_now - anchor_wall_time_ > kMaxExtrapolation) {
    return std::nullopt;
  }

  const base::TimeDelta start_time = pending_words_.front().start_time;
  const base::TimeDelta front = clock.front_timestamp();
  const base::TimeDelta back = clock.back_timestamp();

  // 1. Audio is already playing: due immediately.
  if (start_time < front) {
    return wall_now;
  }

  // 2. Audio is buffered for output: query `AudioClock` for the exact playback
  // due time, accounting for silence padding and variable playback rates.
  if (start_time <= back) {
    return anchor_wall_time_ + clock.TimeUntilPlayback(start_time);
  }

  // 3. Audio has not been written to buffer yet: linearly project the playback
  // due time past the buffer at `rate`. Any silence after the last buffered
  // audio (an underflow) is not counted, so the next `Render()` refreshes the
  // estimate.
  if (rate <= 0.0) {
    return std::nullopt;
  }
  return anchor_wall_time_ + clock.TimeUntilPlayback(back) +
         (start_time - back) / rate;
}

void WordBoundaryQueue::Clear() {
  pending_words_.clear();
  enqueued_duration_ = base::TimeDelta();
  anchor_wall_time_ = base::TimeTicks();
}

}  // namespace readaloud
