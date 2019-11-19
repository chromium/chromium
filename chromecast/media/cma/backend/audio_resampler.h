// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_RESAMPLER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_RESAMPLER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace chromecast {

namespace media {
class DecoderBufferBase;
}  // namespace media

// The audio resampler allows us to apply small changes to the rate of audio
// playback via (supposedly) imperceptible changes.
//
// Note: only works for planar float data.
class AudioResampler {
 public:
  explicit AudioResampler(size_t channel_count);

  // Sets the effective media clock rate.
  double SetMediaClockRate(double rate);

  // Resamples a buffer, adding or dropping frames as necessary to match the
  // media clock rate.
  scoped_refptr<media::DecoderBufferBase> ResampleBuffer(
      scoped_refptr<media::DecoderBufferBase> buffer);

 private:
  // Adds a frame to a buffer.
  scoped_refptr<media::DecoderBufferBase> LengthenBuffer(
      scoped_refptr<media::DecoderBufferBase> buffer);

  // Cuts a frame from a buffer.
  scoped_refptr<media::DecoderBufferBase> ShortenBuffer(
      scoped_refptr<media::DecoderBufferBase> buffer);

  const size_t channel_count_;
  double media_clock_rate_ = 1.0;
  int64_t input_frames_for_clock_rate_ = 0;
  int64_t output_frames_for_clock_rate_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AudioResampler);
};

}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_RESAMPLER_H_
