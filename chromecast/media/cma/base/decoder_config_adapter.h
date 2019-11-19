// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BASE_DECODER_CONFIG_ADAPTER_H_
#define CHROMECAST_MEDIA_CMA_BASE_DECODER_CONFIG_ADAPTER_H_

#include "chromecast/public/media/decoder_config.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"

namespace chromecast {
namespace media {

class DecoderConfigAdapter {
 public:
  static ChannelLayout ToChannelLayout(::media::ChannelLayout channel_layout);
  static ::media::ChannelLayout ToMediaChannelLayout(
      ChannelLayout channel_layout);

  // Converts ::media::AudioDecoderConfig to chromecast::media::AudioConfig.
  static AudioConfig ToCastAudioConfig(
      StreamId id,
      const ::media::AudioDecoderConfig& config);

  // Converts chromecast::media::AudioConfig to ::media::AudioDecoderConfig.
  static ::media::AudioDecoderConfig ToMediaAudioDecoderConfig(
      const AudioConfig& config);

  // Converts ::media::VideoDecoderConfig to chromecast::media::VideoConfig.
  static VideoConfig ToCastVideoConfig(
      StreamId id,
      const ::media::VideoDecoderConfig& config);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BASE_DECODER_CONFIG_ADAPTER_H_
