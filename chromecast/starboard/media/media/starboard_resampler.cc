// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "starboard_resampler.h"

#include <cmath>

#include "base/check_op.h"
#include "base/logging.h"
#include "chromecast/starboard/chromecast/starboard_cast_api/cast_starboard_api_types.h"

namespace chromecast {
namespace media {

namespace {

// A function type for converting an input sample to a double.
using ToDoubleFn = double (*)(const uint8_t* in_iter);

// A function type for converting the double specified by `in` to an output
// sample, written to `out_iter`.
using ToOutputFn = void (*)(double in, uint8_t* out_iter);

// Converts an unsigned 8-bit audio sample to a double.
double U8ToDouble(const uint8_t* in) {
  constexpr int16_t kUnsignedInt8Offset = 0x80;
  // After being shifted to signed 8 bit,the values are in [-128, 127].
  const double max_value = *in > kUnsignedInt8Offset ? 0x7F : 0x80;
  return (*in - kUnsignedInt8Offset) / max_value;
}

// Converts a signed 16-bit audio sample to a double.
double S16ToDouble(const uint8_t* in) {
  const int16_t in_s16 = *reinterpret_cast<const int16_t*>(in);
  const double max_value = in_s16 > 0 ? 0x7FFF : 0x8000;
  return in_s16 / max_value;
}

// Converts a Big Endian signed 16-bit audio sample to a double.
double S16BEToDouble(const uint8_t* in) {
  // Reverse the bytes.
  uint8_t data[2];
  data[0] = in[1];
  data[1] = in[0];

  // Now `data` holds an S16 in little endian byte order, so we can reuse the
  // logic of S16ToDouble.
  return S16ToDouble(data);
}

// Converts a signed 24-bit audio sample to a double.
double S24ToDouble(const uint8_t* in) {
  // Note: we avoid bit shifting here, since bit shifting signed types is
  // undefined behavior, and converting from signed to unsigned types can be
  // implementation-defined.
  uint8_t data[4];
  data[0] = in[0];
  data[1] = in[1];
  data[2] = in[2];
  // The highest byte of the int32_t is filled with the same value as the
  // highest bit of the int24, to properly account for the sign.
  data[3] = ((in[2] & 0x80) == 0) ? 0x00 : ~0x00;
  const int32_t in_s24 = *reinterpret_cast<int32_t*>(data);
  const double max_value = in_s24 > 0 ? 0x7FFFFF : 0x800000;
  return in_s24 / max_value;
}

// Converts a signed 32-bit audio sample to a double.
double S32ToDouble(const uint8_t* in) {
  const int32_t in_s32 = *reinterpret_cast<const int32_t*>(in);
  const double max_value = in_s32 > 0 ? 0x7FFFFFFF : 0x80000000;
  return in_s32 / max_value;
}

// Converts a float audio sample to a double.
double FloatToDouble(const uint8_t* in) {
  return *reinterpret_cast<const float*>(in);
}

// Converts a double audio sample to S16, writing it to `out`.
void WriteDoubleToS16(double in, uint8_t* out) {
  const int32_t max_value = in > 0 ? 0x7FFF : 0x8000;
  *reinterpret_cast<int16_t*>(out) = std::round(in * max_value);
}

// Converts a double audio sample to S32, writing it to `out`.
void WriteDoubleToS32(double in, uint8_t* out) {
  const int64_t max_value = in > 0 ? 0x7FFFFFFF : 0x80000000;
  *reinterpret_cast<int32_t*>(out) =
      std::round(static_cast<double>(in) * max_value);
}

// Converts a double audio sample to float, writing it to `out`.
void WriteDoubleToFloat(double in, uint8_t* out) {
  *reinterpret_cast<float*>(out) = in;
}

// Returns the number of bytes per channel for `format`. `format` must not be
// unknown.
//
// Note that this is different from the implementation of
// ::media::SampleFormatToBitsPerChannel(). That one maps S24 to 4 bytes instead
// of 3.
int BytesPerChannel(SampleFormat format) {
  switch (format) {
    case chromecast::media::kSampleFormatU8: {
      return 1;
    }
    case chromecast::media::kSampleFormatPlanarS16:
    case chromecast::media::kSampleFormatS16: {
      return 2;
    }
    case chromecast::media::kSampleFormatS24: {
      return 3;
    }
    case chromecast::media::kSampleFormatPlanarS32:
    case chromecast::media::kSampleFormatS32:
    case chromecast::media::kSampleFormatF32:
    case chromecast::media::kSampleFormatPlanarF32: {
      return 4;
    }
    default: {
      LOG(FATAL) << "Unsupported sample format: " << format;
    }
  }
}

// Returns the number of bytes per channel for `format`.
int BytesPerChannel(StarboardPcmSampleFormat format) {
  switch (format) {
    case kStarboardPcmSampleFormatS16:
      return 2;
    case kStarboardPcmSampleFormatS32:
    case kStarboardPcmSampleFormatF32:
      return 4;
  }
}

// Returns the conversion function needed to convert `in_sample_format` to
// double. `in_audio_codec` is needed because Big Endian S16 data is treated as
// its own codec (kCodecPCM_S16BE).
//
// This function crashes if an invalid in_sample_format is passed to it.
ToDoubleFn GetInputConversionFunction(SampleFormat in_sample_format,
                                      AudioCodec in_audio_codec) {
  switch (in_sample_format) {
    case chromecast::media::kSampleFormatU8: {
      return &U8ToDouble;
    }
    case chromecast::media::kSampleFormatPlanarS16:
    case chromecast::media::kSampleFormatS16: {
      return (in_audio_codec == kCodecPCM_S16BE) ? &S16BEToDouble
                                                 : &S16ToDouble;
    }
    case chromecast::media::kSampleFormatS24: {
      return &S24ToDouble;
    }
    case chromecast::media::kSampleFormatPlanarS32:
    case chromecast::media::kSampleFormatS32: {
      return &S32ToDouble;
    }
    case chromecast::media::kSampleFormatF32:
    case chromecast::media::kSampleFormatPlanarF32: {
      return &FloatToDouble;
    }
    default: {
      LOG(FATAL) << "Unsupported input format: " << in_sample_format;
    }
  }
}

// Returns the conversion function needed to convert double values to
// `out_sample_format`.
ToOutputFn GetOutputConversionFunction(
    StarboardPcmSampleFormat out_sample_format) {
  switch (out_sample_format) {
    case kStarboardPcmSampleFormatS16: {
      return &WriteDoubleToS16;
    }
    case kStarboardPcmSampleFormatS32: {
      return &WriteDoubleToS32;
    }
    case kStarboardPcmSampleFormatF32: {
      return &WriteDoubleToFloat;
    }
  }
}

// Resamples the PCM data in `buffer` from `in_sample_format` to
// `out_sample_format`. `out_size` is the size of the returned buffer, in bytes.
// `in_audio_codec` is needed because S16BE is treated as its own codec.
std::unique_ptr<uint8_t[]> ResamplePCM(
    const CastDecoderBuffer& buffer,
    int num_channels,
    SampleFormat in_sample_format,
    StarboardPcmSampleFormat out_sample_format,
    AudioCodec in_audio_codec,
    size_t& out_size) {
  DCHECK_GT(num_channels, 0);

  const int in_bytes_per_channel = BytesPerChannel(in_sample_format);
  const int out_bytes_per_channel = BytesPerChannel(out_sample_format);

  const uint8_t* in_data = buffer.data();
  out_size =
      (buffer.data_size() * out_bytes_per_channel) / in_bytes_per_channel;
  auto out_data = std::make_unique<uint8_t[]>(out_size);

  // These two functions are used to convert any input type to any output type.
  // The conversion goes:
  //   input -> double -> output
  // to reduce the number of code paths (2n vs (n choose 2)) and simplify the
  // conversion logic (since converting to/from double is straightforward).
  const ToDoubleFn convert_to_double =
      GetInputConversionFunction(in_sample_format, in_audio_codec);
  const ToOutputFn write_output_type =
      GetOutputConversionFunction(out_sample_format);

  const bool is_input_planar =
      in_sample_format == chromecast::media::kSampleFormatPlanarS16 ||
      in_sample_format == chromecast::media::kSampleFormatPlanarS32 ||
      in_sample_format == chromecast::media::kSampleFormatPlanarF32;

  const int num_planes = is_input_planar ? num_channels : 1;

  std::vector<int> plane_offsets;
  for (int plane = 0; plane < num_planes; ++plane) {
    plane_offsets.push_back(plane * (buffer.data_size() / num_planes));
  }

  // For each input sample, this loop converts it to double and then to the
  // final output format. It also handles converting from planar to interleaved,
  // if necessary.
  //
  // Planar data is grouped by channel; interleaved data is not. For example,
  // consider 2-channel data where A represents data from the first channel, and
  // B represents data from the second channel. 4 samples of planar data would
  // be:
  //   AAAABBBB
  // whereas 4 samples of interleaved data would be:
  //   ABABABAB
  for (int i = 0;
       i * static_cast<size_t>(in_bytes_per_channel) < buffer.data_size();
       ++i) {
    const int plane = i % num_planes;
    const int index_by_plane = i / num_planes;

    write_output_type(convert_to_double(in_data + plane_offsets[plane] +
                                        index_by_plane * in_bytes_per_channel),
                      &out_data[i * out_bytes_per_channel]);
  }

  return out_data;
}

// Returns true if the two formats do not match, meaning resampling is
// necessary. Returns false otherwise.
bool RequiresResampling(StarboardPcmSampleFormat format_to_decode_to,
                        SampleFormat format_to_decode_from) {
  const bool same_format =
      (format_to_decode_to == kStarboardPcmSampleFormatS16 &&
       format_to_decode_from == kSampleFormatS16) ||
      (format_to_decode_to == kStarboardPcmSampleFormatS32 &&
       format_to_decode_from == kSampleFormatS32) ||
      (format_to_decode_to == kStarboardPcmSampleFormatF32 &&
       format_to_decode_from == kSampleFormatF32);
  return !same_format;
}
}  // namespace

std::unique_ptr<uint8_t[]> ResamplePCMAudioDataForStarboard(
    StarboardPcmSampleFormat format_to_decode_to,
    SampleFormat format_to_decode_from,
    AudioCodec audio_codec,
    int audio_channels,
    const CastDecoderBuffer& buffer,
    size_t& buffer_out_size) {
  // The check for kCodecPCM is necessary because there is a separate codec,
  // kCodecPCM_S16BE, which is in Big Endian format instead of Little Endian
  // like all the other formats. In that case, at minimum we need to change the
  // samples to Little Endian, since we do not support Big Endian output
  // formats.
  if (audio_codec == AudioCodec::kCodecPCM &&
      !RequiresResampling(format_to_decode_to, format_to_decode_from)) {
    // No resampling is necessary; return a copy of the data.
    const size_t size_of_buffer = buffer.data_size();
    buffer_out_size = size_of_buffer;
    auto data_copy = std::make_unique<uint8_t[]>(size_of_buffer);
    memcpy(data_copy.get(), buffer.data(), size_of_buffer);
    return data_copy;
  }

  return ResamplePCM(buffer, audio_channels, format_to_decode_from,
                     format_to_decode_to, audio_codec, buffer_out_size);
}

}  // namespace media
}  // namespace chromecast
