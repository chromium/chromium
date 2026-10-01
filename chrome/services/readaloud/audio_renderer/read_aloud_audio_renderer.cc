// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/audio_renderer/read_aloud_audio_renderer.h"

#include <algorithm>

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
  base::AutoLock auto_lock(lock_);
  algorithm_.Initialize(params, /*is_encrypted=*/false);
  algorithm_.SetPreservesPitch(true);
  audio_clock_.emplace(/*start_timestamp=*/base::TimeDelta(),
                       params_.sample_rate());
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

  base::AutoLock auto_lock(lock_);

  // 1. Refill the algorithm's queue from the segment queue if it's not full.
  while (!algorithm_.IsQueueFull()) {
    scoped_refptr<DecodedAudioSegment> segment = queue_->Pop();
    if (!segment) {
      break;
    }
    if (segment->audio_buffer()) {
      algorithm_.EnqueueBuffer(segment->audio_buffer());
    }
  }

  // 2. Call FillBuffer to fill the destination bus.
  const double playback_rate = playback_rate_.load(std::memory_order_relaxed);
  int frames_written =
      algorithm_.FillBuffer(dest, 0, dest->frames(), playback_rate);

  // 3. Zero out any remaining frames if we underflowed.
  if (frames_written < dest->frames()) {
    dest->ZeroFramesPartial(frames_written, dest->frames() - frames_written);
  }

  // 4. Advance the audio clock. `delay` comes from the sink and is clamped
  // because `media::AudioClock::WroteAudio()` CHECKs that it is not negative.
  const base::TimeDelta clamped_delay =
      std::clamp(delay, base::TimeDelta(), kMaxAcceptableDelay);
  const int64_t delay_frames = media::AudioTimestampHelper::TimeToFrames(
      clamped_delay, params_.sample_rate());
  audio_clock_->WroteAudio(frames_written, dest->frames(),
                           base::checked_cast<int>(delay_frames),
                           playback_rate);

  return frames_written;
}

void ReadAloudAudioRenderer::SetPlaybackRate(double rate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  playback_rate_.store(rate, std::memory_order_relaxed);
}

void ReadAloudAudioRenderer::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock auto_lock(lock_);
  algorithm_.FlushBuffers();
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
