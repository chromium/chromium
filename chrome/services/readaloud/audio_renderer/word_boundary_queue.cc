// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/audio_renderer/word_boundary_queue.h"

#include <algorithm>

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

void WordBoundaryQueue::SetAudibleAnchor(base::TimeDelta media_time,
                                         base::TimeTicks wall_time) {
  anchor_media_time_ = media_time;
  anchor_wall_time_ = wall_time;
}

std::optional<WordBoundaryQueue::DueWord> WordBoundaryQueue::PopDueWord(
    base::TimeTicks now,
    double rate) {
  const std::optional<base::TimeTicks> due_time = FrontWordDueTime(now, rate);
  if (!due_time || *due_time > now) {
    return std::nullopt;
  }
  DueWord word{
      .timing = pending_words_.front(),
      .audio_timestamp = anchor_media_time_ + (now - anchor_wall_time_) * rate,
  };
  pending_words_.pop_front();
  // TODO(b/524284442): Log the average delay between highlighting a word and
  // its audio becoming audible.
  return word;
}

std::optional<base::TimeDelta> WordBoundaryQueue::TimeUntilNextWordDue(
    base::TimeTicks now,
    double rate) const {
  const std::optional<base::TimeTicks> due_time = FrontWordDueTime(now, rate);
  if (!due_time) {
    return std::nullopt;
  }

  return std::clamp(*due_time - now, base::TimeDelta(), kMaxExtrapolation);
}

std::optional<base::TimeTicks> WordBoundaryQueue::FrontWordDueTime(
    base::TimeTicks now,
    double rate) const {
  if (pending_words_.empty() || rate <= 0.0) {
    return std::nullopt;
  }

  // Staleness guard. An old anchor means playback is paused, stalled or
  // finished.
  if (now - anchor_wall_time_ > kMaxExtrapolation) {
    return std::nullopt;
  }

  // TODO(b/527525845): This extrapolation assumes all audio after the anchor
  // plays at `rate` with no gap. It ignores the output-delay silence the audio
  // clock queues after a start or flush, and audio still buffered at an older
  // rate after a rate change, so dispatch can be early by up to the output
  // delay. Compute the due time with media::AudioClock::TimeUntilPlayback()
  // instead.
  return anchor_wall_time_ +
         (pending_words_.front().start_time - anchor_media_time_) / rate;
}

void WordBoundaryQueue::Clear() {
  pending_words_.clear();
  enqueued_duration_ = base::TimeDelta();
  anchor_media_time_ = base::TimeDelta();
  anchor_wall_time_ = base::TimeTicks();
}

}  // namespace readaloud
