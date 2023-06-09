// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_RECORDING_AUDIO_CAPTURE_TEST_BASE_H_
#define CHROMEOS_ASH_SERVICES_RECORDING_AUDIO_CAPTURE_TEST_BASE_H_

#include "base/time/time.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_parameters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace recording {

// Defines a base class test fixture for testing audio streams and their mixing.
class AudioCaptureTestBase : public testing::Test {
 public:
  AudioCaptureTestBase();
  AudioCaptureTestBase(const AudioCaptureTestBase&) = delete;
  AudioCaptureTestBase& operator=(const AudioCaptureTestBase&) = delete;
  ~AudioCaptureTestBase() override = default;

  // Returns a `TimeTicks` that is offset from origin by `delay`.
  static base::TimeTicks GetTimestamp(base::TimeDelta delay);

  // Returns true if both `bus1` and `bus2` has the same number of channels,
  // frames, and the values of all the frames are equal.
  static bool AreBusesEqual(const media::AudioBus& bus1,
                            const media::AudioBus& bus2);

  // Creates and returns an audio bus that matches the `audio_parameters_` after
  // it uses the `SineWaveAudioSource` to fill it with data.
  std::unique_ptr<media::AudioBus> ProduceAudio(base::TimeTicks timestamp);

 protected:
  media::AudioParameters audio_parameters_;
  media::SineWaveAudioSource audio_source_;
};

}  // namespace recording

#endif  // CHROMEOS_ASH_SERVICES_RECORDING_AUDIO_CAPTURE_TEST_BASE_H_
