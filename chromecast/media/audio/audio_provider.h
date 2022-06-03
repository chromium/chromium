// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_AUDIO_PROVIDER_H_
#define CHROMECAST_MEDIA_AUDIO_AUDIO_PROVIDER_H_

#include <cstdint>

namespace chromecast {
namespace media {

// Abstract interface for classes that provide audio data.
class AudioProvider {
 public:
  // Fills in |channel_data| with up to |num_frames| frames of audio.
  // The |playout_timestamp| indicates when the first sample of the filled audio
  // is expected to play out. Returns the number of frames actually filled;
  // implementations should try to fill as much audio as possible.
  virtual int FillFrames(int num_frames,
                         int64_t playout_timestamp,
                         float* const* channel_data) = 0;

  // Returns the number of audio channels and the sample rate of the provider.
  // Used for DCHECKing only; all callers of a provider must use the same
  // channel count and sample rate as the provider.
  virtual size_t num_channels() const = 0;
  virtual int sample_rate() const = 0;

 protected:
  virtual ~AudioProvider() = default;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_AUDIO_PROVIDER_H_
