// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_API_CAST_CHANNEL_MIXER_H_
#define CHROMECAST_MEDIA_API_CAST_CHANNEL_MIXER_H_

#include <memory>

#include "chromecast/public/media/decoder_config.h"

namespace chromecast {
namespace media {

// Channel mixer to convert audio between different channel layouts.
class CastChannelMixer {
 public:
  static std::unique_ptr<CastChannelMixer> Create(ChannelLayout input,
                                                  ChannelLayout output);

  virtual ~CastChannelMixer() = default;

  // Change the channel counts for |input|. |input| should be prepared in planar
  // float format. The returned value points to an array that is owned by this
  // class, also in planar float format. The returned data is only valid until
  // the next call.
  virtual const float* Transform(const float* input, int num_frames) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_API_CAST_CHANNEL_MIXER_H_
