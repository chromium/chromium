// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_API_CAST_AUDIO_RESAMPLER_H_
#define CHROMECAST_MEDIA_API_CAST_AUDIO_RESAMPLER_H_

#include <memory>
#include <vector>

namespace chromecast {
namespace media {

// Audio resampler interface.
class CastAudioResampler {
 public:
  // Creates a CastAudioResampler instance.
  static std::unique_ptr<CastAudioResampler> Create(int channel_count,
                                                    int input_sample_rate,
                                                    int output_sample_rate);

  virtual ~CastAudioResampler() = default;

  // Resamples |input|, which is assumed to be in planar float format, appending
  // the resampled audio in planar float format into |output_channels|, which is
  // an array of |channel_count| vectors (one per channel). Note that some
  // audio from |input| may be stored internally and will not be output until
  // the next call to Resample() or Flush().
  virtual void Resample(const float* input,
                        int num_frames,
                        std::vector<float>* output_channels) = 0;

  // Resamples any internally-buffered input audio, filling with silence as
  // necessary. The resampled audio is appended in planar float format into
  // |output_channels|, which is an array of |channel_count| vectors (one per
  // channel).
  virtual void Flush(std::vector<float>* output_channels) = 0;

  // Returns the number of input frames that are buffered internally (ie, have
  // not yet been resampled into output).
  virtual int BufferedInputFrames() const = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_API_CAST_AUDIO_RESAMPLER_H_
