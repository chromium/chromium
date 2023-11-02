// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_bus.h"

#include <cstring>

#include "base/memory/ptr_util.h"

namespace chromecast {
namespace media {

CastAudioBus::CastAudioBus(int channels, int frames) : frames_(frames) {
  data_.reset(new float[channels * frames]);
  channel_data_.reserve(channels);
  for (int i = 0; i < channels; ++i)
    channel_data_.push_back(data_.get() + i * frames);
}

CastAudioBus::~CastAudioBus() = default;

// static
std::unique_ptr<CastAudioBus> CastAudioBus::Create(int channels, int frames) {
  return base::WrapUnique(new CastAudioBus(channels, frames));
}

void CastAudioBus::Zero() {
  std::fill_n(data_.get(), frames() * channels(), 0);
}

}  // namespace media
}  // namespace chromecast
