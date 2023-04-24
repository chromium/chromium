// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/slew_volume.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "base/check_op.h"
#include "media/base/vector_math.h"

namespace {

// The time to slew from current volume to target volume.
const int kMaxSlewTimeMs = 100;
const int kDefaultSampleRate = 44100;

}  // namespace

struct FMACTraits {
  static void ProcessBulkData(const float* src,
                              float volume,
                              int frames,
                              float* dest) {
    ::media::vector_math::FMAC(src, volume, frames, dest);
  }

  static void ProcessSingleDatum(const float* src, float volume, float* dest) {
    (*dest) += (*src) * volume;
  }

  static void ProcessZeroVolume(const float* src, int frames, float* dest) {}

  static void ProcessUnityVolume(const float* src, int frames, float* dest) {
    ProcessBulkData(src, 1.0, frames, dest);
  }
};

struct FMULTraits {
  static void ProcessBulkData(const float* src,
                              float volume,
                              int frames,
                              float* dest) {
    ::media::vector_math::FMUL(src, volume, frames, dest);
  }

  static void ProcessSingleDatum(const float* src, float volume, float* dest) {
    (*dest) = (*src) * volume;
  }

  static void ProcessZeroVolume(const float* src, int frames, float* dest) {
    std::memset(dest, 0, frames * sizeof(*dest));
  }

  static void ProcessUnityVolume(const float* src, int frames, float* dest) {
    if (src == dest) {
      return;
    }
    std::memcpy(dest, src, frames * sizeof(*dest));
  }
};

namespace chromecast {
namespace media {

SlewVolume::SlewVolume() : SlewVolume(kMaxSlewTimeMs) {}

SlewVolume::SlewVolume(int max_slew_time_ms)
    : SlewVolume(max_slew_time_ms, false) {}

SlewVolume::SlewVolume(int max_slew_time_ms, bool use_cosine_slew)
    : sample_rate_(kDefaultSampleRate),
      max_slew_time_ms_(max_slew_time_ms),
      max_slew_per_sample_(1000.0 / (max_slew_time_ms_ * sample_rate_)),
      use_cosine_slew_(use_cosine_slew) {}

void SlewVolume::SetSampleRate(int sample_rate) {
  CHECK_GT(sample_rate, 0);

  sample_rate_ = sample_rate;
  SetVolume(volume_scale_);
}

void SlewVolume::SetVolume(double volume_scale) {
  volume_scale_ = volume_scale;
  if (interrupted_) {
    current_volume_ = volume_scale_;
    last_starting_volume_ = current_volume_;
  }

  // Slew rate should be volume_to_slew / slew_time / sample_rate, but use a
  // minimum volume_to_slew of 0.1 to avoid very small slew per sample.
  double volume_diff =
      std::max(0.1, std::fabs(volume_scale_ - current_volume_));
  max_slew_per_sample_ =
      volume_diff * 1000.0 / (max_slew_time_ms_ * sample_rate_);

  if (use_cosine_slew_) {
    // Set initial state for cosine slew. Cosine fading always lasts
    // max_slew_time_ms_.
    slew_counter_ = max_slew_time_ms_ * 0.001 * sample_rate_;
    slew_angle_ = sin(M_PI / slew_counter_);
    slew_offset_ = (current_volume_ + volume_scale_) * 0.5;
    slew_cos_ = (current_volume_ - volume_scale_) * 0.5;
    slew_sin_ = 0.0;
  }
}

float SlewVolume::LastBufferMaxMultiplier() {
  return std::max(current_volume_, last_starting_volume_);
}

void SlewVolume::SetMaxSlewTimeMs(int max_slew_time_ms) {
  CHECK_GE(max_slew_time_ms, 0);

  max_slew_time_ms_ = max_slew_time_ms;
}

void SlewVolume::Interrupted() {
  interrupted_ = true;
  current_volume_ = volume_scale_;
}

void SlewVolume::ProcessFMAC(bool repeat_transition,
                             const float* src,
                             int frames,
                             int channels,
                             float* dest) {
  ProcessData<FMACTraits>(repeat_transition, src, frames, channels, dest);
}

void SlewVolume::ProcessFMUL(bool repeat_transition,
                             const float* src,
                             int frames,
                             int channels,
                             float* dest) {
  ProcessData<FMULTraits>(repeat_transition, src, frames, channels, dest);
}

template <typename Traits>
void SlewVolume::ProcessData(bool repeat_transition,
                             const float* src,
                             int frames,
                             int channels,
                             float* dest) {
  DCHECK(src);
  DCHECK(dest);
  // Ensure |src| and |dest| are 16-byte aligned.
  DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(src) &
                    (::media::vector_math::kRequiredAlignment - 1));
  DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(dest) &
                    (::media::vector_math::kRequiredAlignment - 1));

  if (!frames) {
    return;
  }

  interrupted_ = false;
  if (repeat_transition) {
    current_volume_ = last_starting_volume_;
  } else {
    last_starting_volume_ = current_volume_;
  }

  if (current_volume_ == volume_scale_) {
    if (current_volume_ == 0.0) {
      Traits::ProcessZeroVolume(src, frames * channels, dest);
      return;
    }
    if (current_volume_ == 1.0) {
      Traits::ProcessUnityVolume(src, frames * channels, dest);
      return;
    }
    Traits::ProcessBulkData(src, current_volume_, frames * channels, dest);
    return;
  }

  if (use_cosine_slew_) {
    int slew_frames = std::min(slew_counter_, frames);
    frames -= slew_frames;
    slew_counter_ -= slew_frames;
    for (; slew_frames > 0; --slew_frames) {
      slew_cos_ -= slew_sin_ * slew_angle_;
      slew_sin_ += slew_cos_ * slew_angle_;
      current_volume_ = std::clamp(slew_offset_ + slew_cos_, 0.0, 1.0);
      for (int i = 0; i < channels; ++i) {
        Traits::ProcessSingleDatum(src, current_volume_, dest);
        ++src;
        ++dest;
      }
    }
    if (!slew_counter_) {
      current_volume_ = volume_scale_;
    }
  } else if (current_volume_ < volume_scale_) {
    do {
      for (int i = 0; i < channels; ++i) {
        Traits::ProcessSingleDatum(src, current_volume_, dest);
        ++src;
        ++dest;
      }
      --frames;
      current_volume_ += max_slew_per_sample_;
    } while (current_volume_ < volume_scale_ && frames);
    current_volume_ = std::min(current_volume_, volume_scale_);
  } else {  // current_volume_ > volume_scale_
    do {
      for (int i = 0; i < channels; ++i) {
        Traits::ProcessSingleDatum(src, current_volume_, dest);
        ++src;
        ++dest;
      }
      --frames;
      current_volume_ -= max_slew_per_sample_;
    } while (current_volume_ > volume_scale_ && frames);
    current_volume_ = std::max(current_volume_, volume_scale_);
  }
  while (frames && (reinterpret_cast<uintptr_t>(src) &
                    (::media::vector_math::kRequiredAlignment - 1))) {
    for (int i = 0; i < channels; ++i) {
      Traits::ProcessSingleDatum(src, current_volume_, dest);
      ++src;
      ++dest;
    }
    --frames;
  }
  if (!frames) {
    return;
  }
  Traits::ProcessBulkData(src, current_volume_, frames * channels, dest);
}

}  // namespace media
}  // namespace chromecast
