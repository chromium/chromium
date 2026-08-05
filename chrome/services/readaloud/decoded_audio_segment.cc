// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/decoded_audio_segment.h"

#include <utility>

#include "base/check.h"
#include "media/base/audio_buffer.h"

namespace readaloud {

DecodedAudioSegment::DecodedAudioSegment() = default;

DecodedAudioSegment::DecodedAudioSegment(base::TimeDelta duration)
    : duration_(duration) {}

DecodedAudioSegment::DecodedAudioSegment(
    scoped_refptr<media::AudioBuffer> audio_buffer,
    std::vector<DecodedAudioSegment::WordTiming> word_timings)
    : audio_buffer_(std::move(audio_buffer)),
      word_timings_(std::move(word_timings)) {
  DCHECK(audio_buffer_);
  DCHECK(!audio_buffer_->end_of_stream());
}

DecodedAudioSegment::~DecodedAudioSegment() = default;

}  // namespace readaloud
