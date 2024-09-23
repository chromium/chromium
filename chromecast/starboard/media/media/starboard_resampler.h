// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_RESAMPLER_H_
#define CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_RESAMPLER_H_

#include <stdint.h>
#include <stdio.h>

#include <memory>

#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/starboard/chromecast/starboard_cast_api/cast_starboard_api_types.h"

namespace chromecast {
namespace media {

// Returns resampled PCM data. `format_to_decode_to` is the output format, and
// `format_to_decode_from` is the input format. `audio_codec` describes the
// input codec, and is used to handle the case of Big Endian input data,
// kCodecPCM_S16BE. `audio_channels` is the number of channels in the input
// data, and must be greater than 0. `buffer` contains the input PCM data.
// `buffer_out_size` is set to the size of the output buffer, in bytes.
//
// TODO(b/334991778): see if we can reuse existing chromium infra to do the
// conversion.
std::unique_ptr<uint8_t[]> ResamplePCMAudioDataForStarboard(
    StarboardPcmSampleFormat format_to_decode_to,
    SampleFormat format_to_decode_from,
    AudioCodec audio_codec,
    int audio_channels,
    const CastDecoderBuffer& buffer,
    size_t& buffer_out_size);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_RESAMPLER_H_
