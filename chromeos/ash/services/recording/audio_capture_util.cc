// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chromeos/ash/services/recording/audio_capture_util.h"

#include "base/memory/aligned_memory.h"
#include "base/numerics/safe_conversions.h"
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
bool CanUseVectorMath(base::span<const float> src,
                      base::span<const float> dest) {
  return base::IsAligned(src.data(), media::vector_math::kRequiredAlignment) &&
         base::IsAligned(dest.data(), media::vector_math::kRequiredAlignment);
}

// If `media::vector_math::FMAC()` cannot be used due to lack of required
// alignment, this version can be used to accumulate the `length` number of
// items from `src` on top of the values existing in `dest`.
void Accumulate(base::span<const float> src, base::span<float> dest) {
  for (size_t i = 0; i < src.size(); ++i) {
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
                     int destination_start_frame,
                     int length) {
  CHECK_EQ(source.channels(), source.channels());
  CHECK_LE(length, source.frames());
  CHECK_LE(destination_start_frame + length, destination->frames());

  const size_t dest_offset =
      base::checked_cast<size_t>(destination_start_frame);
  const size_t count = base::checked_cast<size_t>(length);

  for (int i = 0; i < source.channels(); ++i) {
    auto src = source.channel_span(i).first(count);
    auto dest = destination->channel_span(i).subspan(dest_offset, count);
    if (CanUseVectorMath(src, dest)) {
      media::vector_math::FMAC(src, /*scale=*/1, dest);
    } else {
      Accumulate(src, dest);
    }
  }
}

}  // namespace recording::audio_capture_util
