// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSORS_GOVERNOR_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSORS_GOVERNOR_H_

#include <memory>
#include <string>
#include <vector>

#include "chromecast/media/base/slew_volume.h"
#include "chromecast/public/media/audio_post_processor2_shlib.h"

namespace chromecast {
namespace media {

// Provides linear reduction in output volume if the input volume is above a
// given threshold.
// Used to protect speakers at high output levels while providing dynamic range
// at low output level.
// The configuration string for this plugin is:
//  {"onset_volume": |VOLUME_TO_CLAMP|, "clamp_multiplier": |CLAMP_MULTIPLIER|}
// Input volumes > |VOLUME_TO_CLAMP| will be attenuated by linear approximation,
// changing from 0 (at VOLUME_TO_CLAMP) to |CLAMP_MULTIPLIER| (at 1.0).
// |CLAMP_MULTIPLIER| must be >= |VOLUME_TO_CLAMP|.
class Governor : public AudioPostProcessor2 {
 public:
  Governor(const std::string& config, int input_channels);

  Governor(const Governor&) = delete;
  Governor& operator=(const Governor&) = delete;

  ~Governor() override;

  // AudioPostProcessor2 implementation:
  bool SetConfig(const Config& config) override;
  const Status& GetStatus() override;
  void ProcessFrames(float* data, int frames, Metadata* metadata) override;
  bool UpdateParameters(const std::string& message) override;

  void SetSlewTimeMsForTest(int slew_time_ms);

 private:
  float GetGovernorMultiplier();

  Status status_;
  float volume_;
  double onset_volume_;
  double clamp_multiplier_;
  SlewVolume slew_volume_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSORS_GOVERNOR_H_
