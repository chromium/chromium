// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSORS_GOVERNOR_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSORS_GOVERNOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chromecast/media/base/slew_volume.h"
#include "chromecast/public/media/audio_post_processor2_shlib.h"

namespace chromecast {
namespace media {

// Provides a flat reduction in output volume if the input volume is above a
// given threshold.
// Used to protect speakers at high output levels while providing dynamic range
// at low output level.
// The configuration string for this plugin is:
//  {"onset_volume": |VOLUME_TO_CLAMP|, "clamp_multiplier": |CLAMP_MULTIPLIER|}
// Input volumes > |VOLUME_TO_CLAMP| will be attenuated by |CLAMP_MULTIPLIER|.
class Governor : public AudioPostProcessor2 {
 public:
  Governor(const std::string& config, int input_channels);
  ~Governor() override;

  // AudioPostProcessor2 implementation:
  bool SetConfig(const Config& config) override;
  const Status& GetStatus() override;
  void ProcessFrames(float* data,
                     int frames,
                     float cast_volume,
                     float volume_dbfs) override;
  bool UpdateParameters(const std::string& message) override;

  void SetSlewTimeMsForTest(int slew_time_ms);

 private:
  float GetGovernorMultiplier();

  Status status_;
  float volume_;
  double onset_volume_;
  double clamp_multiplier_;
  SlewVolume slew_volume_;

  DISALLOW_COPY_AND_ASSIGN(Governor);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSORS_GOVERNOR_H_
