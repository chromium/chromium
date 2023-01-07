// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Scale volume with slew rate limiting

#ifndef CHROMECAST_MEDIA_BASE_SLEW_VOLUME_H_
#define CHROMECAST_MEDIA_BASE_SLEW_VOLUME_H_

#include <stdint.h>

namespace chromecast {
namespace media {

class SlewVolume {
 public:
  SlewVolume();
  explicit SlewVolume(int max_slew_time_ms);
  // Use raised negative cosine function when |use_cosine_slew| is true and
  // linear otherwise.
  SlewVolume(int max_slew_time_ms, bool use_cosine_slew);

  SlewVolume(const SlewVolume&) = delete;
  SlewVolume& operator=(const SlewVolume&) = delete;

  ~SlewVolume() = default;

  void SetSampleRate(int sample_rate);
  void SetVolume(double volume_scale);

  // Return the largest multiplier that was applied in the last call to
  // ProcessFMAC() or ProcessFMUL(). The largest multiplier is used because
  // that determines the largest possible value in the buffer.
  float LastBufferMaxMultiplier();
  void SetMaxSlewTimeMs(int max_slew_time_ms);

  // Called to indicate that the stream was interrupted; volume changes can be
  // applied immediately.
  void Interrupted();

  // Smoothly calculates dest[i] += src[i] * |volume_scale|.
  // |volume_scale| will always be consistent across a frame.
  // |src| and |dest| are interleaved buffers with |channels| channels and at
  // least |frames| frames (|channels| * |frames| total size).
  // |src| and |dest| may be the same.
  // |src| and |dest| must be 16-byte aligned.
  // If using planar data, |repeat_transition| should be true for channels 2
  // through n, which will cause the slewing process to be repeated.
  void ProcessFMAC(bool repeat_transition,
                   const float* src,
                   int frames,
                   int channels,
                   float* dest);

  // Smoothly calculates dest[i] = src[i] * |volume_scale|.
  // |volume_scale| will always be consistent across a frame.
  // |src| and |dest| are interleaved buffers with |channels| channels and at
  // least |frames| frames (|channels| * |frames| total size).
  // |src| and |dest| may be the same.
  // |src| and |dest| must be 16-byte aligned.
  // If using planar data, |repeat_transition| should be true for channels 2
  // through n, which will cause the slewing process to be repeated.
  void ProcessFMUL(bool repeat_transition,
                   const float* src,
                   int frames,
                   int channels,
                   float* dest);

 private:
  template <typename Traits>
  void ProcessData(bool repeat_transition,
                   const float* src,
                   int frames,
                   int channels,
                   float* dest);

  double sample_rate_;
  double volume_scale_ = 1.0;
  double current_volume_ = 1.0;
  double last_starting_volume_ = 1.0;
  double max_slew_time_ms_;
  double max_slew_per_sample_;
  bool interrupted_ = true;
  bool use_cosine_slew_ = false;
  int slew_counter_;
  double slew_angle_;
  double slew_offset_;
  double slew_cos_;
  double slew_sin_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_SLEW_VOLUME_H_
