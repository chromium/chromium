// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/audio_renderer/read_aloud_audio_renderer.h"

#include <algorithm>

#include "chrome/services/readaloud/audio_segment_queue.h"
#include "chrome/services/readaloud/decoded_audio_segment.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"

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
  algorithm_.Initialize(params, /*is_encrypted=*/false);
  algorithm_.SetPreservesPitch(true);
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
  int frames_written = algorithm_.FillBuffer(
      dest, 0, dest->frames(), playback_rate_.load(std::memory_order_relaxed));

  // 3. Zero out any remaining frames if we underflowed.
  if (frames_written < dest->frames()) {
    dest->ZeroFramesPartial(frames_written, dest->frames() - frames_written);
  }

  return frames_written;
}

void ReadAloudAudioRenderer::SetPlaybackRate(double rate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  playback_rate_.store(rate, std::memory_order_relaxed);
}

void ReadAloudAudioRenderer::OnRenderError() {
  // TODO(b/524283367): Handle render errors in later phases.
}

}  // namespace readaloud
