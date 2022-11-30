// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_API_AUDIO_CLOCK_SIMULATOR_H_
#define CHROMECAST_MEDIA_API_AUDIO_CLOCK_SIMULATOR_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "chromecast/media/api/audio_provider.h"

namespace chromecast {
namespace media {

// Simulates a modifiable audio output clock rate by resampling. Note that this
// will always provide audio (FillFrames() always fills the entire buffer),
// even if the upstream provider does not provide any data.
class AudioClockSimulator : public AudioProvider {
 public:
  static std::unique_ptr<AudioClockSimulator> Create(AudioProvider* provider);

  // Sets the simulated audio clock rate. Returns the effective rate.
  virtual double SetRate(double rate) = 0;

  // Returns the number of frames of additional delay due to audio stored
  // internally.
  virtual double DelayFrames() const = 0;

  // Sets a new playback sample rate. Needed to calculate timestamps correctly.
  virtual void SetSampleRate(int sample_rate) = 0;

  // Sets the playback rate (rate at which samples are played out relative to
  // the sample rate). Needed to calculate timestamps correctly.
  virtual void SetPlaybackRate(double playback_rate) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_API_AUDIO_CLOCK_SIMULATOR_H_
