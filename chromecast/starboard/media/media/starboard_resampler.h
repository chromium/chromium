// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_RESAMPLER_H_
#define CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_RESAMPLER_H_

#include <cstddef>
#include <cstdint>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/starboard/chromecast/starboard_cast_api/cast_starboard_api_types.h"
#include "media/base/audio_codecs.h"
#include "media/base/sample_format.h"

namespace chromecast {
namespace media {

// Returns resampled PCM data. `format_to_decode_to` is the output format, and
// `format_to_decode_from` is the input format. `audio_codec` describes the
// input codec, and is used to handle the case of Big Endian input data,
// kCodecPCM_S16BE. `audio_channels` is the number of channels in the input
// data, and must be greater than 0. `in_data` contains the input PCM data.
// Returns an empty array if the codec, sample format, channel count, or buffer
// size is invalid or unsupported.
//
// TODO: crbug.com/323610278 - remove this when cast no longer uses CMA.
base::HeapArray<uint8_t> ResamplePCMAudioDataForStarboard(
    StarboardPcmSampleFormat format_to_decode_to,
    SampleFormat format_to_decode_from,
    AudioCodec audio_codec,
    int audio_channels,
    base::span<const uint8_t> in_data);

// Same as above, but uses chromium enums instead of cast ones.
base::HeapArray<uint8_t> ResamplePCMAudioDataForStarboard(
    StarboardPcmSampleFormat format_to_decode_to,
    ::media::SampleFormat format_to_decode_from,
    ::media::AudioCodec audio_codec,
    int audio_channels,
    base::span<const uint8_t> in_data);

// Returns true if the PCM buffer byte size is non-zero, frame-aligned for the
// input sample format and channel count, and can be resampled to the output
// format without size overflow.
bool IsValidPcmBufferSize(StarboardPcmSampleFormat format_to_decode_to,
                          SampleFormat format_to_decode_from,
                          int audio_channels,
                          size_t byte_size);

// Same as above, using Chromium media enums.
bool IsValidPcmBufferSize(StarboardPcmSampleFormat format_to_decode_to,
                          ::media::SampleFormat format_to_decode_from,
                          int audio_channels,
                          size_t byte_size);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_RESAMPLER_H_
