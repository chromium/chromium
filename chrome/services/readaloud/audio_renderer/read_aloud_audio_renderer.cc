// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/audio_renderer/read_aloud_audio_renderer.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/services/readaloud/audio_segment_queue.h"
#include "chrome/services/readaloud/decoded_audio_segment.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"

namespace readaloud {

ReadAloudAudioRenderer::ReadAloudAudioRenderer() : algorithm_(&media_log_) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ReadAloudAudioRenderer::~ReadAloudAudioRenderer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool ReadAloudAudioRenderer::Initialize(const media::AudioParameters& params,
                                        AudioSegmentQueue* queue) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!initialized_);
  if (!params.IsValid() || !queue) {
    return false;
  }
  params_ = params;
  queue_ = queue;
  task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  base::AutoLock auto_lock(lock_);
  algorithm_.Initialize(params, /*is_encrypted=*/false);
  algorithm_.SetPreservesPitch(true);
  audio_clock_.emplace(/*start_timestamp=*/base::TimeDelta(),
                       params_.sample_rate());
  weak_this_ = weak_factory_.GetWeakPtr();
  initialized_ = true;
  return true;
}

int ReadAloudAudioRenderer::Render(base::TimeDelta delay,
                                   base::TimeTicks delay_timestamp,
                                   const media::AudioGlitchInfo& glitch_info,
                                   media::AudioBus* dest) {
  // Omit DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_) because Render()
  // runs on a dedicated real-time audio thread, not the owning sequence.

  // Defensive check for initialization.
  if (!initialized_ || !queue_) {
    dest->Zero();
    return 0;
  }

  int frames_written = 0;
  bool post_pump = false;
  base::WeakPtr<ReadAloudAudioRenderer> pump_target;
  {
    base::AutoLock auto_lock(lock_);

    // 1. Refill the algorithm's queue from the segment queue if it's not full,
    // handing each segment's word timings to `word_boundaries_` along with the
    // duration of its audio.
    while (!algorithm_.IsQueueFull()) {
      scoped_refptr<DecodedAudioSegment> segment = queue_->Pop();
      if (!segment) {
        break;
      }
      scoped_refptr<media::AudioBuffer> buffer = segment->audio_buffer();
      if (!buffer) {
        // Nothing is played for this segment, so its timings are never
        // audible and it takes up no media time.
        continue;
      }
      word_boundaries_.Enqueue(segment->word_timings(), buffer->duration());
      algorithm_.EnqueueBuffer(std::move(buffer));
    }

    // 2. Call FillBuffer to fill the destination bus.
    const double playback_rate = playback_rate_.load(std::memory_order_relaxed);
    frames_written =
        algorithm_.FillBuffer(dest, 0, dest->frames(), playback_rate);

    // 3. Zero out any remaining frames if we underflowed.
    if (frames_written < dest->frames()) {
      dest->ZeroFramesPartial(frames_written, dest->frames() - frames_written);
    }

    // 4. Advance the audio clock. `delay` comes from the sink and is clamped
    // because `media::AudioClock::WroteAudio()` CHECKs that it is not
    // negative.
    const base::TimeDelta clamped_delay =
        std::clamp(delay, base::TimeDelta(), kMaxAcceptableDelay);
    const int64_t delay_frames = media::AudioTimestampHelper::TimeToFrames(
        clamped_delay, params_.sample_rate());
    audio_clock_->WroteAudio(frames_written, dest->frames(),
                             base::checked_cast<int>(delay_frames),
                             playback_rate);

    // 5. Anchor the media clock to the wall clock. `delay_timestamp` is the
    // instant the sink paired with `delay`, so it is steadier than sampling
    // base::TimeTicks::Now() here, and avoids a clock read on the real-time
    // thread.
    word_boundaries_.SetAudibleAnchor(audio_clock_->front_timestamp(),
                                      delay_timestamp);

    // 6. Decide whether to wake the pump. Boundaries are deliberately not
    // detected here: noticing that a word has already started would report it
    // up to one render quantum late. The pump instead extrapolates from the
    // anchor above and dispatches each word when it becomes audible.
    if (!word_boundaries_.empty() && !pump_task_posted_) {
      pump_task_posted_ = true;
      post_pump = true;
      pump_target = weak_this_;
    }
  }

  // Posted after releasing `lock_`, which keeps the allocation and the task
  // queue's own lock out of the critical section. If Flush() runs in between,
  // it has already invalidated `pump_target`, so the task is dropped.
  // `pump_target` is only copied here: its validity may only be checked on the
  // owning sequence.
  if (post_pump) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ReadAloudAudioRenderer::PumpWordBoundaries,
                                  std::move(pump_target)));
  }

  return frames_written;
}

void ReadAloudAudioRenderer::PumpWordBoundaries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const double rate = playback_rate_.load(std::memory_order_relaxed);

  // Dispatch every word that is already audible, then sleep until the next one
  // is. `word_boundary_callback_` is run outside `lock_`.
  while (true) {
    std::optional<WordBoundaryQueue::DueWord> word;
    std::optional<base::TimeDelta> wait;
    {
      base::AutoLock auto_lock(lock_);
      // Cleared on every run, so a Render() racing with this pump may post a
      // redundant one. That is harmless: each run recomputes everything from
      // the latest anchor.
      pump_task_posted_ = false;
      const base::TimeTicks now = base::TimeTicks::Now();
      word = word_boundaries_.PopDueWord(now, rate);
      if (!word) {
        wait = word_boundaries_.TimeUntilNextWordDue(now, rate);
        pump_task_posted_ = wait.has_value();
      }
    }

    // Tasks are posted and callbacks run outside `lock_`, so that the
    // real-time Render() never waits on them.
    if (word) {
      if (word_boundary_callback_) {
        word_boundary_callback_.Run(word->timing.start_character_offset,
                                    word->timing.end_character_offset,
                                    word->audio_timestamp);
      }
      continue;
    }
    if (wait) {
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ReadAloudAudioRenderer::PumpWordBoundaries,
                         weak_factory_.GetWeakPtr()),
          *wait);
    }
    // Otherwise there is nothing to wait for; the next Render() restarts the
    // pump.
    return;
  }
}

void ReadAloudAudioRenderer::SetWordBoundaryCallback(
    WordBoundaryCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  word_boundary_callback_ = std::move(callback);
}

void ReadAloudAudioRenderer::SetPlaybackRate(double rate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  playback_rate_.store(rate, std::memory_order_relaxed);
  // A pump sleeping on a delay computed at the old rate is not woken: it
  // recomputes its due time when it fires, and
  // `WordBoundaryQueue::kMaxExtrapolation` caps how long that takes.
  // TODO(b/527525845): Reschedule the pending pump here once the due time
  // accounts for audio still buffered at the old rate. Until then a word can
  // be dispatched late by up to kMaxExtrapolation after a speed-up.
}

void ReadAloudAudioRenderer::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock auto_lock(lock_);
  algorithm_.FlushBuffers();
  // Drop any pump still pending, and the boundaries it would have dispatched.
  weak_factory_.InvalidateWeakPtrs();
  weak_this_ = weak_factory_.GetWeakPtr();
  pump_task_posted_ = false;
  word_boundaries_.Clear();
  if (initialized_) {
    audio_clock_.emplace(/*start_timestamp=*/base::TimeDelta(),
                         params_.sample_rate());
  }
}

base::TimeDelta ReadAloudAudioRenderer::GetMediaTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock auto_lock(lock_);
  return audio_clock_ ? audio_clock_->front_timestamp() : base::TimeDelta();
}

void ReadAloudAudioRenderer::OnRenderError() {
  // TODO(b/524283367): Handle render errors in later phases.
}

}  // namespace readaloud
