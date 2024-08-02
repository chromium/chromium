// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/services/recording/audio_capture_test_base.h"

#include "chromeos/ash/services/recording/audio_capture_util.h"

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

  for (int i = 0; i < bus1.channels(); ++i) {
    const auto* const bus1_channel = bus1.channel(i);
    const auto* const bus2_channel = bus2.channel(i);
    for (int j = 0; j < bus1.frames(); ++j) {
      if (bus1_channel[j] != bus2_channel[j]) {
        return false;
      }
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
