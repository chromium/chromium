// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/decoded_audio_segment.h"

#include <utility>

#include "base/check.h"
#include "media/base/audio_bus.h"

namespace readaloud {

DecodedAudioSegment::DecodedAudioSegment() = default;

DecodedAudioSegment::DecodedAudioSegment(base::TimeDelta duration)
    : duration_(duration) {}

DecodedAudioSegment::DecodedAudioSegment(
    std::unique_ptr<media::AudioBus> audio_bus,
    int sample_rate,
    base::TimeDelta duration,
    std::vector<DecodedAudioSegment::WordTiming> word_timings)
    : audio_bus_(std::move(audio_bus)),
      sample_rate_(sample_rate),
      duration_(duration),
      word_timings_(std::move(word_timings)) {
  DCHECK(audio_bus_);
  DCHECK(!audio_bus_->is_bitstream_format());
}

DecodedAudioSegment::~DecodedAudioSegment() = default;

}  // namespace readaloud
