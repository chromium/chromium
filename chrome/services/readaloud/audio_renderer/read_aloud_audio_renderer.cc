// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/audio_renderer/read_aloud_audio_renderer.h"

#include "chrome/services/readaloud/audio_segment_queue.h"
#include "media/base/audio_bus.h"

namespace readaloud {

ReadAloudAudioRenderer::ReadAloudAudioRenderer() {
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

  // TODO(b/524283367): Handle actual PCM frame copying logic. Currently
  // filling with zeroes to allow compilation.
  dest->Zero();
  return 0;
}

void ReadAloudAudioRenderer::OnRenderError() {
  // TODO(b/524283367): Handle render errors in later phases.
}

}  // namespace readaloud
