// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/post_processors/saturated_gain.h"

#include <algorithm>
#include <cmath>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromecast/media/base/slew_volume.h"
#include "chromecast/media/cma/backend/mixer/post_processor_registry.h"

namespace chromecast {
namespace media {

namespace {

const char kGainKey[] = "gain_db";

float DbFsToScale(float db) {
  return std::pow(10, db / 20);
}

}  // namespace

SaturatedGain::SaturatedGain(const std::string& config, int channels)
    : last_volume_dbfs_(-1) {
  status_.output_channels = channels;
  status_.ringing_time_frames = 0;
  status_.rendering_delay_frames = 0;
  auto config_dict = base::JSONReader::ReadDict(config);
  CHECK(config_dict) << "SaturatedGain config is not valid json: " << config;
  auto gain_db = config_dict->FindDouble(kGainKey);
  CHECK(gain_db) << config;
  gain_ = DbFsToScale(*gain_db);
  LOG(INFO) << "Created a SaturatedGain: gain = " << *gain_db << "db";
}

SaturatedGain::~SaturatedGain() = default;

bool SaturatedGain::SetConfig(const AudioPostProcessor2::Config& config) {
  status_.input_sample_rate = config.output_sample_rate;
  slew_volume_.SetSampleRate(status_.input_sample_rate);
  return true;
}

const AudioPostProcessor2::Status& SaturatedGain::GetStatus() {
  return status_;
}

void SaturatedGain::ProcessFrames(float* data, int frames, Metadata* metadata) {
  DCHECK(data);

  status_.output_buffer = data;
  if (metadata->volume_dbfs != last_volume_dbfs_) {
    last_volume_dbfs_ = metadata->volume_dbfs;
    // Don't apply more gain than attenuation.
    float effective_gain = std::min(DbFsToScale(-last_volume_dbfs_), gain_);
    slew_volume_.SetVolume(effective_gain);
  }

  slew_volume_.ProcessFMUL(false, data, frames, status_.output_channels, data);
}

bool SaturatedGain::UpdateParameters(const std::string& message) {
  return false;
}

REGISTER_POSTPROCESSOR(SaturatedGain, "libcast_saturated_gain_2.0.so");

}  // namespace media
}  // namespace chromecast
