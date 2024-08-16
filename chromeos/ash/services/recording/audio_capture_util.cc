// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/services/recording/audio_capture_util.h"

#include "base/memory/aligned_memory.h"
#include "chromeos/ash/services/recording/recording_service_constants.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/vector_math.h"

namespace recording::audio_capture_util {

namespace {

static_assert(kAudioSampleRate % 100 == 0,
              "Audio sample rate is not divisible by 100");

// Using `media::vector_math::FMAC()` works only if the addresses of `src` and
// `dest` are `kRequiredAlignment` bit aligned.
// This returns true if that's the case.
bool CanUseVectorMath(const float* src, const float* dest) {
  return base::IsAligned(src, media::vector_math::kRequiredAlignment) &&
         base::IsAligned(dest, media::vector_math::kRequiredAlignment);
}

// If `media::vector_math::FMAC()` cannot be used due to lack of required
// alignment, this version can be used to accumulate the `length` number of
// items from `src` on top of the values existing in `dest`.
void Accumulate(const float* src, float* dest, int length) {
  for (int i = 0; i < length; ++i) {
    dest[i] += src[i];
  }
}

}  // namespace

media::AudioParameters GetAudioCaptureParameters() {
  return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(),
                                kAudioSampleRate, kAudioSampleRate / 100);
}

int64_t NumberOfAudioFramesInDuration(base::TimeDelta duration) {
  return media::AudioTimestampHelper::TimeToFrames(duration, kAudioSampleRate);
}

std::unique_ptr<media::AudioBus> CreateStereoZeroInitializedAudioBusForFrames(
    int64_t frames) {
  auto bus = media::AudioBus::Create(
      media::ChannelLayoutConfig::Stereo().channels(), frames);
  bus->Zero();
  return bus;
}

std::unique_ptr<media::AudioBus> CreateStereoZeroInitializedAudioBusForDuration(
    base::TimeDelta duration) {
  return CreateStereoZeroInitializedAudioBusForFrames(
      NumberOfAudioFramesInDuration(duration));
}

void AccumulateBusTo(const media::AudioBus& source,
                     media::AudioBus* destination,
                     int source_start_frame,
                     int destination_start_frame,
                     int length) {
  CHECK_EQ(source.channels(), source.channels());
  CHECK_LE(source_start_frame + length, source.frames());
  CHECK_LE(destination_start_frame + length, destination->frames());

  for (int i = 0; i < source.channels(); ++i) {
    const float* src = &source.channel(i)[source_start_frame];
    float* dest = &destination->channel(i)[destination_start_frame];
    if (CanUseVectorMath(src, dest)) {
      media::vector_math::FMAC(src, /*scale=*/1, length, dest);
    } else {
      Accumulate(src, dest, length);
    }
  }
}

}  // namespace recording::audio_capture_util
