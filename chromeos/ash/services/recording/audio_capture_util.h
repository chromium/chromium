// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_RECORDING_AUDIO_CAPTURE_UTIL_H_
#define CHROMEOS_ASH_SERVICES_RECORDING_AUDIO_CAPTURE_UTIL_H_

#include <cstdint>
#include <memory>

#include "base/time/time.h"

namespace media {
class AudioBus;
class AudioParameters;
}  // namespace media

namespace recording::audio_capture_util {

// The requested audio sample rate of the audio capturer.
constexpr int kAudioSampleRate = 48000;

// If any of the `AudioStream`s that are managed by the `AudioStreamMixer` has
// a number of frames that collectively last for a duration that is equal or
// exceeding the below `kMaxAudioStreamFifoDuration`, the mixer will consume and
// mix all the mixable frames from all the streams, even if one or more streams
// are completely empty.
constexpr base::TimeDelta kMaxAudioStreamFifoDuration = base::Seconds(2);

// Returns the audio parameters that gets used globally by the recording
// service. The returned parameters specify using stereo channel, PCM low
// latency audio recording at the `kAudioSampleRate`, and a number of frames per
// buffer that is equal to `kAudioSampleRate` / 100.
media::AudioParameters GetAudioCaptureParameters();

// Returns the number of frames that collectively last for the given `duration`
// at the capture `kAudioSampleRate`.
int64_t NumberOfAudioFramesInDuration(base::TimeDelta duration);

// Creates a stereo audio bus that has the given number of `frames`, whose
// values are zeros.
std::unique_ptr<media::AudioBus> CreateStereoZeroInitializedAudioBusForFrames(
    int64_t frames);

// Creates a stereo audio bus that has a number of frames that collectively last
// for the given `duration` at the capture `kAudioSampleRate`.
std::unique_ptr<media::AudioBus> CreateStereoZeroInitializedAudioBusForDuration(
    base::TimeDelta duration);

// Accumulates the `length` number of audio frames in the `source` audio bus
// starting at `source_start_frame`, to the already existing frames in the
// `destination` bus starting at `destination_start_frame`.
// Both `source` and `destination` buses must have the same number of channels.
// `source_start_frame` + `length` must be within the bounds of the `source`
// bus, and `destination_start_frame` + `length` also must be within the bounds
// of the `destination` bus.
void AccumulateBusTo(const media::AudioBus& source,
                     media::AudioBus* destination,
                     int source_start_frame,
                     int destination_start_frame,
                     int length);

}  // namespace recording::audio_capture_util

#endif  // CHROMEOS_ASH_SERVICES_RECORDING_AUDIO_CAPTURE_UTIL_H_
