// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/post_processors/governor.h"

#include <limits>
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/numerics/ranges.h"
#include "base/values.h"
#include "chromecast/base/serializers.h"
#include "chromecast/media/base/slew_volume.h"

namespace chromecast {
namespace media {

namespace {

const int kNoVolume = -1;
const float kEpsilon = std::numeric_limits<float>::epsilon();

// Configuration strings:
const char kOnsetVolumeKey[] = "onset_volume";
const char kClampMultiplierKey[] = "clamp_multiplier";

}  // namespace

Governor::Governor(const std::string& config, int input_channels)
    : volume_(kNoVolume), slew_volume_() {
  status_.output_channels = input_channels;
  status_.rendering_delay_frames = 0;
  status_.ringing_time_frames = 0;
  auto config_dict = base::DictionaryValue::From(DeserializeFromJson(config));
  CHECK(config_dict) << "Governor config is not valid json: " << config;
  CHECK(config_dict->GetDouble(kOnsetVolumeKey, &onset_volume_));
  CHECK(config_dict->GetDouble(kClampMultiplierKey, &clamp_multiplier_));
  slew_volume_.SetVolume(1.0);
  LOG(INFO) << "Created a governor: onset_volume = " << onset_volume_
            << ", clamp_multiplier = " << clamp_multiplier_;
}

Governor::~Governor() = default;

bool Governor::SetConfig(const AudioPostProcessor2::Config& config) {
  status_.input_sample_rate = config.output_sample_rate;
  slew_volume_.SetSampleRate(status_.input_sample_rate);
  return true;
}

const AudioPostProcessor2::Status& Governor::GetStatus() {
  return status_;
}

void Governor::ProcessFrames(float* data,
                             int frames,
                             float volume,
                             float volume_dbfs) {
  DCHECK(data);
  status_.output_buffer = data;

  // If the volume has changed.
  if (!base::IsApproximatelyEqual(volume, volume_, kEpsilon)) {
    volume_ = volume;
    slew_volume_.SetVolume(GetGovernorMultiplier());
  }

  slew_volume_.ProcessFMUL(false, data, frames, status_.output_channels, data);
}

float Governor::GetGovernorMultiplier() {
  // If |volume_| is greater than or "equal" to |onset_volume_|.
  if (volume_ > onset_volume_ - kEpsilon) {
    return clamp_multiplier_;
  }
  return 1.0;
}

bool Governor::UpdateParameters(const std::string& message) {
  return false;
}

void Governor::SetSlewTimeMsForTest(int slew_time_ms) {
  slew_volume_.SetMaxSlewTimeMs(slew_time_ms);
}

}  // namespace media
}  // namespace chromecast
