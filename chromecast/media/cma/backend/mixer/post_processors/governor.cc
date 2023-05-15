// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/post_processors/governor.h"

#include <limits>
#include <memory>
#include <string>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/numerics/ranges.h"
#include "base/values.h"
#include "chromecast/media/base/slew_volume.h"
#include "chromecast/media/cma/backend/mixer/post_processor_registry.h"

namespace chromecast {
namespace media {

namespace {

const int kNoVolume = -1;
const float kEpsilon = std::numeric_limits<float>::epsilon();
const float kMaxOnSetVolume = 0.989;  // -0.1dB

// Configuration strings:
const char kOnsetVolumeKey[] = "onset_volume";
const char kClampMultiplierKey[] = "clamp_multiplier";

}  // namespace

Governor::Governor(const std::string& config, int input_channels)
    : volume_(kNoVolume) {
  status_.output_channels = input_channels;
  status_.rendering_delay_frames = 0;
  status_.ringing_time_frames = 0;
  auto config_dict = base::JSONReader::ReadDict(config);
  CHECK(config_dict) << "Governor config is not valid json: " << config;
  auto onset_volume = config_dict->FindDouble(kOnsetVolumeKey);
  CHECK(onset_volume);
  onset_volume_ = onset_volume.value();
  auto clamp_multiplier = config_dict->FindDouble(kClampMultiplierKey);
  CHECK(clamp_multiplier);
  clamp_multiplier_ = clamp_multiplier.value();
  CHECK_LE(onset_volume_, clamp_multiplier_);
  CHECK_LE(onset_volume_, kMaxOnSetVolume);
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

void Governor::ProcessFrames(float* data, int frames, Metadata* metadata) {
  DCHECK(data);
  status_.output_buffer = data;

  // If the volume has changed.
  if (!base::IsApproximatelyEqual(metadata->system_volume, volume_, kEpsilon)) {
    volume_ = metadata->system_volume;
    slew_volume_.SetVolume(GetGovernorMultiplier());
  }

  slew_volume_.ProcessFMUL(false, data, frames, status_.output_channels, data);
}

float Governor::GetGovernorMultiplier() {
  if (volume_ > onset_volume_) {
    float effective_volume =
        onset_volume_ + (volume_ - onset_volume_) *
                            (clamp_multiplier_ - onset_volume_) /
                            (1. - onset_volume_);
    return effective_volume / volume_;
  }
  return 1.0;
}

bool Governor::UpdateParameters(const std::string& message) {
  return false;
}

void Governor::SetSlewTimeMsForTest(int slew_time_ms) {
  slew_volume_.SetMaxSlewTimeMs(slew_time_ms);
}

REGISTER_POSTPROCESSOR(Governor, "libcast_governor_2.0.so");

}  // namespace media
}  // namespace chromecast
