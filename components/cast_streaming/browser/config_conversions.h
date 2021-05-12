// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_CONFIG_CONVERSIONS_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_CONFIG_CONVERSIONS_H_

#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "third_party/openscreen/src/cast/streaming/capture_configs.h"

namespace cast_streaming {

// Utility functions to convert between media and Open Screen types.

openscreen::cast::AudioCaptureConfig AudioDecoderConfigToAudioCaptureConfig(
    const media::AudioDecoderConfig& audio_config);

openscreen::cast::VideoCaptureConfig VideoDecoderConfigToVideoCaptureConfig(
    const media::VideoDecoderConfig& video_config);

media::AudioDecoderConfig AudioCaptureConfigToAudioDecoderConfig(
    const openscreen::cast::AudioCaptureConfig& audio_capture_config);

media::VideoDecoderConfig VideoCaptureConfigToVideoDecoderConfig(
    const openscreen::cast::VideoCaptureConfig& video_capture_config);

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_CONFIG_CONVERSIONS_H_
