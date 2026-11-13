// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/media/starboard_resampler.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_math.h"
#include "chromecast/starboard/chromecast/starboard_cast_api/cast_starboard_api_types.h"

namespace chromecast {
namespace media {

namespace {

// A function type for converting an input sample to a double.
using ToDoubleFn = double (*)(base::span<const uint8_t> in);

// A function type for converting the double specified by `in` to an output
// sample, written to `out`.
using ToOutputFn = void (*)(double in, base::span<uint8_t> out);

// Converts an unsigned 8-bit audio sample to a double.
double U8ToDouble(base::span<const uint8_t> in) {
  constexpr int16_t kUnsignedInt8Offset = 0x80;
  // After being shifted to signed 8 bit,the values are in [-128, 127].
  const double max_value = in[0] > kUnsignedInt8Offset ? 0x7F : 0x80;
  return (in[0] - kUnsignedInt8Offset) / max_value;
}

// Converts a signed 16-bit audio sample to a double.
double S16ToDouble(base::span<const uint8_t> in) {
  const int16_t in_s16 = base::I16FromLittleEndian(in.first<2>());
  const double max_value = in_s16 > 0 ? 0x7FFF : 0x8000;
  return in_s16 / max_value;
}

// Converts a Big Endian signed 16-bit audio sample to a double.
double S16BEToDouble(base::span<const uint8_t> in) {
  const int16_t in_s16 = base::I16FromBigEndian(in.first<2>());
  const double max_value = in_s16 > 0 ? 0x7FFF : 0x8000;
  return in_s16 / max_value;
}

// Converts a signed 24-bit audio sample to a double.
double S24ToDouble(base::span<const uint8_t> in) {
  // Note: we avoid bit shifting here, since bit shifting signed types is
  // undefined behavior, and converting from signed to unsigned types can be
  // implementation-defined.
  std::array<uint8_t, 4> data;
  data[0] = in[0];
  data[1] = in[1];
  data[2] = in[2];
  // The highest byte of the int32_t is filled with the same value as the
  // highest bit of the int24, to properly account for the sign.
  data[3] = ((in[2] & 0x80) == 0) ? 0x00 : ~0x00;
  const int32_t in_s24 = base::I32FromLittleEndian(data);
  const double max_value = in_s24 > 0 ? 0x7FFFFF : 0x800000;
  return in_s24 / max_value;
}

// Converts a signed 32-bit audio sample to a double.
double S32ToDouble(base::span<const uint8_t> in) {
  const int32_t in_s32 = base::I32FromLittleEndian(in.first<4>());
  const double max_value = in_s32 > 0 ? 0x7FFFFFFF : 0x80000000;
  return in_s32 / max_value;
}

// Converts a float audio sample to a double.
double FloatToDouble(base::span<const uint8_t> in) {
  return base::FloatFromLittleEndian(in.first<4>());
}

// Converts a double audio sample to S16, writing it to `out`.
void WriteDoubleToS16(double in, base::span<uint8_t> out) {
  const int32_t max_value = in > 0 ? 0x7FFF : 0x8000;
  out.take_first<2>().copy_from(base::byte_span_from_ref(
      static_cast<int16_t>(std::round(in * max_value))));
}

// Converts a double audio sample to S32, writing it to `out`.
void WriteDoubleToS32(double in, base::span<uint8_t> out) {
  const int64_t max_value = in > 0 ? 0x7FFFFFFF : 0x80000000;
  out.take_first<4>().copy_from(base::byte_span_from_ref(
      static_cast<int32_t>(std::round(in * max_value))));
}

// Converts a double audio sample to float, writing it to `out`.
void WriteDoubleToFloat(double in, base::span<uint8_t> out) {
  out.take_first<4>().copy_from(base::byte_span_from_ref(
      base::allow_nonunique_obj, static_cast<float>(in)));
}

// Returns the number of bytes per channel for the given format, or nullopt if
// unsupported. Note that this differs from
// ::media::SampleFormatToBitsPerChannel(), which maps S24 to 4 bytes instead of
// 3.
std::optional<size_t> TryGetBytesPerChannel(SampleFormat format) {
  switch (format) {
    case chromecast::media::kSampleFormatU8:
      return 1u;
    case chromecast::media::kSampleFormatPlanarS16:
    case chromecast::media::kSampleFormatS16:
      return 2u;
    case chromecast::media::kSampleFormatS24:
      return 3u;
    case chromecast::media::kSampleFormatPlanarS32:
    case chromecast::media::kSampleFormatS32:
    case chromecast::media::kSampleFormatF32:
    case chromecast::media::kSampleFormatPlanarF32:
      return 4u;
    default:
      return std::nullopt;
  }
}

std::optional<size_t> TryGetBytesPerChannel(StarboardPcmSampleFormat format) {
  switch (format) {
    case kStarboardPcmSampleFormatS16:
      return 2u;
    case kStarboardPcmSampleFormatS32:
    case kStarboardPcmSampleFormatF32:
      return 4u;
    default:
      return std::nullopt;
  }
}

// Returns the conversion function needed to convert `in_sample_format` to
// double. `in_audio_codec` is needed because Big Endian S16 data is treated as
// its own codec (kCodecPCM_S16BE).
//
// Returns nullptr if an unsupported in_sample_format is passed to it.
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
      LOG(ERROR) << "Unsupported input format: " << in_sample_format;
      return nullptr;
    }
  }
}

// Returns the conversion function needed to convert double values to
// `out_sample_format`.
//
// Returns nullptr if an unsupported out_sample_format is passed to it.
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
    default: {
      LOG(ERROR) << "Unsupported output format: " << out_sample_format;
      return nullptr;
    }
  }
}

// Resamples the PCM data in `buffer` from `in_sample_format` to
// `out_sample_format`. `in_audio_codec` is needed because S16BE is treated as
// its own codec.
base::HeapArray<uint8_t> ResamplePCM(base::span<const uint8_t> in_data,
                                     int num_channels,
                                     SampleFormat in_sample_format,
                                     StarboardPcmSampleFormat out_sample_format,
                                     AudioCodec in_audio_codec) {
  if (!IsValidPcmBufferSize(out_sample_format, in_sample_format, num_channels,
                            in_data.size())) {
    LOG(ERROR) << "Invalid PCM input parameters for resampling: byte_size="
               << in_data.size() << ", channels=" << num_channels
               << ", in_format=" << in_sample_format
               << ", out_format=" << out_sample_format;
    return {};
  }

  const std::optional<size_t> in_bytes_per_channel =
      TryGetBytesPerChannel(in_sample_format);
  const std::optional<size_t> out_bytes_per_channel =
      TryGetBytesPerChannel(out_sample_format);
  // These two functions are used to convert any input type to any output type.
  // The conversion goes:
  //   input -> double -> output
  // to reduce the number of code paths (2n vs (n choose 2)) and simplify the
  // conversion logic (since converting to/from double is straightforward).
  const ToDoubleFn convert_to_double =
      GetInputConversionFunction(in_sample_format, in_audio_codec);
  const ToOutputFn write_output_type =
      GetOutputConversionFunction(out_sample_format);
  if (!in_bytes_per_channel.has_value() || !out_bytes_per_channel.has_value() ||
      convert_to_double == nullptr || write_output_type == nullptr) {
    LOG(ERROR) << "Unsupported PCM conversion formats: in_format="
               << in_sample_format << ", out_format=" << out_sample_format;
    return {};
  }

  const size_t total_samples = in_data.size() / *in_bytes_per_channel;
  const base::CheckedNumeric<size_t> out_size =
      (*in_bytes_per_channel == *out_bytes_per_channel)
          ? base::CheckedNumeric<size_t>(in_data.size())
          : base::CheckMul(total_samples, *out_bytes_per_channel);
  if (!out_size.IsValid()) {
    LOG(ERROR) << "PCM output buffer size overflow.";
    return {};
  }

  auto out_data = base::HeapArray<uint8_t>::WithSize(out_size.ValueOrDie());

  const bool is_input_planar =
      in_sample_format == chromecast::media::kSampleFormatPlanarS16 ||
      in_sample_format == chromecast::media::kSampleFormatPlanarS32 ||
      in_sample_format == chromecast::media::kSampleFormatPlanarF32;

  const size_t num_planes =
      is_input_planar ? static_cast<size_t>(num_channels) : 1u;

  std::vector<size_t> plane_offsets;
  plane_offsets.reserve(num_planes);
  for (size_t plane = 0; plane < num_planes; ++plane) {
    plane_offsets.push_back(plane * (in_data.size() / num_planes));
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
  for (size_t i = 0; i < total_samples; ++i) {
    const size_t plane = i % num_planes;
    const size_t index_by_plane = i / num_planes;

    write_output_type(
        convert_to_double(in_data.subspan(
            plane_offsets[plane] + index_by_plane * *in_bytes_per_channel,
            *in_bytes_per_channel)),
        out_data.subspan(i * *out_bytes_per_channel, *out_bytes_per_channel));
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

std::optional<SampleFormat> TryToCastSampleFormat(
    ::media::SampleFormat format) {
  switch (format) {
    case ::media::kSampleFormatU8:
      return kSampleFormatU8;
    case ::media::kSampleFormatS16:
      return kSampleFormatS16;
    case ::media::kSampleFormatS24:
      return kSampleFormatS24;
    case ::media::kSampleFormatS32:
      return kSampleFormatS32;
    case ::media::kSampleFormatF32:
      return kSampleFormatF32;
    case ::media::kSampleFormatPlanarU8:
      return kSampleFormatPlanarU8;
    case ::media::kSampleFormatPlanarS16:
      return kSampleFormatPlanarS16;
    case ::media::kSampleFormatPlanarF32:
      return kSampleFormatPlanarF32;
    case ::media::kSampleFormatPlanarS32:
      return kSampleFormatPlanarS32;
    default:
      return std::nullopt;
  }
}

// Converts supported Chromium PCM AudioCodec values to Cast AudioCodec.
//
// TODO(crbug.com/323610278): remove this after deprecating CMA.
std::optional<AudioCodec> TryToCastAudioCodec(::media::AudioCodec codec) {
  switch (codec) {
    case ::media::AudioCodec::kPCM:
      return kCodecPCM;
    case ::media::AudioCodec::kPCM_S16BE:
      return kCodecPCM_S16BE;
    default:
      LOG(ERROR) << "Unsupported PCM audio codec: " << codec << " ("
                 << static_cast<int>(codec) << ")";
      return std::nullopt;
  }
}

}  // namespace

base::HeapArray<uint8_t> ResamplePCMAudioDataForStarboard(
    StarboardPcmSampleFormat format_to_decode_to,
    SampleFormat format_to_decode_from,
    AudioCodec audio_codec,
    int audio_channels,
    base::span<const uint8_t> in_data) {
  if (audio_codec != AudioCodec::kCodecPCM &&
      audio_codec != AudioCodec::kCodecPCM_S16BE) {
    LOG(ERROR) << "Unsupported PCM audio codec: " << audio_codec;
    return {};
  }
  if (!IsValidPcmBufferSize(format_to_decode_to, format_to_decode_from,
                            audio_channels, in_data.size())) {
    LOG(ERROR) << "Invalid PCM buffer size or format: byte_size="
               << in_data.size() << ", channels=" << audio_channels
               << ", in_format=" << format_to_decode_from
               << ", out_format=" << format_to_decode_to;
    return {};
  }

  // The check for kCodecPCM is necessary because there is a separate codec,
  // kCodecPCM_S16BE, which is in Big Endian format instead of Little Endian
  // like all the other formats. In that case, at minimum we need to change the
  // samples to Little Endian, since we do not support Big Endian output
  // formats.
  if (audio_codec == AudioCodec::kCodecPCM &&
      !RequiresResampling(format_to_decode_to, format_to_decode_from)) {
    // No resampling is necessary; return a copy of the data.
    return base::HeapArray<uint8_t>::CopiedFrom(in_data);
  }

  return ResamplePCM(in_data, audio_channels, format_to_decode_from,
                     format_to_decode_to, audio_codec);
}

base::HeapArray<uint8_t> ResamplePCMAudioDataForStarboard(
    StarboardPcmSampleFormat format_to_decode_to,
    ::media::SampleFormat format_to_decode_from,
    ::media::AudioCodec audio_codec,
    int audio_channels,
    base::span<const uint8_t> in_data) {
  const std::optional<SampleFormat> cast_format =
      TryToCastSampleFormat(format_to_decode_from);
  if (!cast_format.has_value()) {
    LOG(ERROR) << "Unsupported ::media::SampleFormat: "
               << format_to_decode_from;
    return {};
  }
  const std::optional<AudioCodec> cast_codec = TryToCastAudioCodec(audio_codec);
  if (!cast_codec.has_value()) {
    return {};
  }
  return ResamplePCMAudioDataForStarboard(format_to_decode_to, *cast_format,
                                          *cast_codec, audio_channels, in_data);
}

bool IsValidPcmBufferSize(StarboardPcmSampleFormat format_to_decode_to,
                          SampleFormat format_to_decode_from,
                          int audio_channels,
                          size_t byte_size) {
  if (byte_size == 0 || audio_channels <= 0) {
    return false;
  }

  const std::optional<size_t> in_bytes_per_channel =
      TryGetBytesPerChannel(format_to_decode_from);
  const std::optional<size_t> out_bytes_per_channel =
      TryGetBytesPerChannel(format_to_decode_to);
  if (!in_bytes_per_channel.has_value() || !out_bytes_per_channel.has_value()) {
    return false;
  }

  const base::CheckedNumeric<size_t> frame_bytes = base::CheckMul(
      *in_bytes_per_channel, static_cast<size_t>(audio_channels));
  if (!frame_bytes.IsValid()) {
    return false;
  }
  const size_t frame_byte_size = frame_bytes.ValueOrDie();
  if (byte_size % frame_byte_size != 0) {
    return false;
  }

  if (*in_bytes_per_channel == *out_bytes_per_channel) {
    return true;
  }
  const size_t total_samples = byte_size / *in_bytes_per_channel;
  return base::CheckMul(total_samples, *out_bytes_per_channel).IsValid();
}

bool IsValidPcmBufferSize(StarboardPcmSampleFormat format_to_decode_to,
                          ::media::SampleFormat format_to_decode_from,
                          int audio_channels,
                          size_t byte_size) {
  const std::optional<SampleFormat> cast_sample_format =
      TryToCastSampleFormat(format_to_decode_from);
  if (!cast_sample_format.has_value()) {
    return false;
  }
  return IsValidPcmBufferSize(format_to_decode_to, *cast_sample_format,
                              audio_channels, byte_size);
}

}  // namespace media
}  // namespace chromecast
