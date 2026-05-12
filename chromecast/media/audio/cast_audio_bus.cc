// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_bus.h"

#include <algorithm>
#include <cstring>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/checked_math.h"

namespace chromecast {
namespace media {

CastAudioBus::CastAudioBus(int channels, int frames) : frames_(frames) {
  CHECK_GE(channels, 0);
  CHECK_GE(frames, 0);
  size_t size = base::CheckMul(static_cast<size_t>(channels),
                               static_cast<size_t>(frames))
                    .ValueOrDie();
  data_.reset(new float[size]);
  channel_data_.reserve(channels);
  for (int i = 0; i < channels; ++i)
    channel_data_.push_back(UNSAFE_TODO(data_.get() + static_cast<size_t>(i) * frames));
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
