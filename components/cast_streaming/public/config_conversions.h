// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_PUBLIC_CONFIG_CONVERSIONS_H_
#define COMPONENTS_CAST_STREAMING_PUBLIC_CONFIG_CONVERSIONS_H_

#include <vector>

#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "third_party/openscreen/src/cast/streaming/capture_configs.h"

namespace cast_streaming {

// Utility functions to convert between media and Open Screen types.

openscreen::cast::AudioCaptureConfig ToAudioCaptureConfig(
    const media::AudioDecoderConfig& audio_config);

openscreen::cast::VideoCaptureConfig ToVideoCaptureConfig(
    const media::VideoDecoderConfig& video_config);

media::AudioDecoderConfig ToAudioDecoderConfig(
    const openscreen::cast::AudioCaptureConfig& audio_capture_config);

media::VideoDecoderConfig ToVideoDecoderConfig(
    const openscreen::cast::VideoCaptureConfig& video_capture_config);

openscreen::cast::AudioCodec ToAudioCaptureConfigCodec(media::AudioCodec codec);

openscreen::cast::VideoCodec ToVideoCaptureConfigCodec(media::VideoCodec codec);

template <typename... TCodecs>
std::vector<openscreen::cast::AudioCodec> ToAudioCaptureConfigCodecs(
    TCodecs... codecs) {
  return std::vector<openscreen::cast::AudioCodec>{
      ToAudioCaptureConfigCodec(codecs)...};
}

template <typename... TCodecs>
std::vector<openscreen::cast::VideoCodec> ToVideoCaptureConfigCodecs(
    TCodecs... codecs) {
  return std::vector<openscreen::cast::VideoCodec>{
      ToVideoCaptureConfigCodec(codecs)...};
}

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_PUBLIC_CONFIG_CONVERSIONS_H_
