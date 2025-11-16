// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/audio_capture_test_base.h"

#include "base/types/zip.h"
#include "chromeos/ash/services/recording/audio_capture_util.h"
#include "media/base/audio_bus.h"

namespace recording {

AudioCaptureTestBase::AudioCaptureTestBase()
    : audio_parameters_(audio_capture_util::GetAudioCaptureParameters()),
      audio_source_(audio_parameters_.channels(),
                    /*freq=*/audio_parameters_.GetBufferDuration().ToHz(),
                    audio_capture_util::kAudioSampleRate) {}

// static
base::TimeTicks AudioCaptureTestBase::GetTimestamp(base::TimeDelta delay) {
  return base::TimeTicks() + delay;
}

// static
bool AudioCaptureTestBase::AreBusesEqual(const media::AudioBus& bus1,
                                         const media::AudioBus& bus2) {
  if (bus1.channels() != bus2.channels() || bus1.frames() != bus2.frames()) {
    return false;
  }

  for (const auto [bus1_ch, bus2_ch] :
       base::zip(bus1.AllChannels(), bus2.AllChannels())) {
    if (bus1_ch != bus2_ch) {
      return false;
    }
  }

  return true;
}

std::unique_ptr<media::AudioBus> AudioCaptureTestBase::ProduceAudio(
    base::TimeTicks timestamp) {
  auto bus = media::AudioBus::Create(audio_parameters_);
  audio_source_.OnMoreData(/*delay=*/base::TimeDelta(), timestamp,
                           /*glitch_info=*/{}, bus.get());
  return bus;
}

}  // namespace recording
