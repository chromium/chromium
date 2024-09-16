// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/decoder_config_adapter.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "chromecast/media/base/media_codec_support.h"
#include "media/base/channel_layout.h"
#include "media/base/encryption_pattern.h"
#include "media/base/encryption_scheme.h"
#include "ui/gfx/hdr_metadata.h"

namespace chromecast {
namespace media {

namespace {

// Converts ::media::AudioCodec to chromecast::media::AudioCodec. Any unknown or
// unsupported codec will be converted to chromecast::media::kCodecUnknown.
AudioCodec ToAudioCodec(const ::media::AudioCodec audio_codec) {
  switch (audio_codec) {
    case ::media::AudioCodec::kAAC:
      return kCodecAAC;
    case ::media::AudioCodec::kMP3:
      return kCodecMP3;
    case ::media::AudioCodec::kPCM:
      return kCodecPCM;
    case ::media::AudioCodec::kPCM_S16BE:
      return kCodecPCM_S16BE;
    case ::media::AudioCodec::kVorbis:
      return kCodecVorbis;
    case ::media::AudioCodec::kOpus:
      return kCodecOpus;
    case ::media::AudioCodec::kFLAC:
      return kCodecFLAC;
    case ::media::AudioCodec::kEAC3:
      return kCodecEAC3;
    case ::media::AudioCodec::kAC3:
      return kCodecAC3;
    case ::media::AudioCodec::kMpegHAudio:
      return kCodecMpegHAudio;
    case ::media::AudioCodec::kDTS:
      return kCodecDTS;
    case ::media::AudioCodec::kDTSXP2:
      return kCodecDTSXP2;
    case ::media::AudioCodec::kDTSE:
      return kCodecDTSE;
    default:
      LOG(ERROR) << "Unsupported audio codec " << audio_codec;
  }
  return kAudioCodecUnknown;
}

SampleFormat ToSampleFormat(const ::media::SampleFormat sample_format) {
  switch (sample_format) {
    case ::media::kUnknownSampleFormat:
    case ::media::kSampleFormatAc3:
    case ::media::kSampleFormatEac3:
    case ::media::kSampleFormatDts:
    case ::media::kSampleFormatDtsxP2:
    case ::media::kSampleFormatMpegHAudio:
    case ::media::kSampleFormatIECDts:
    case ::media::kSampleFormatDtse:
      return kUnknownSampleFormat;
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
  }
  NOTREACHED();
}

::media::SampleFormat ToMediaSampleFormat(const SampleFormat sample_format) {
  switch (sample_format) {
    case kUnknownSampleFormat:
      return ::media::kUnknownSampleFormat;
    case kSampleFormatU8:
      return ::media::kSampleFormatU8;
    case kSampleFormatS16:
      return ::media::kSampleFormatS16;
    case kSampleFormatS24:
      return ::media::kSampleFormatS24;
    case kSampleFormatS32:
      return ::media::kSampleFormatS32;
    case kSampleFormatF32:
      return ::media::kSampleFormatF32;
    case kSampleFormatPlanarU8:
      return ::media::kSampleFormatPlanarU8;
    case kSampleFormatPlanarS16:
      return ::media::kSampleFormatPlanarS16;
    case kSampleFormatPlanarF32:
      return ::media::kSampleFormatPlanarF32;
    case kSampleFormatPlanarS32:
      return ::media::kSampleFormatPlanarS32;
    default:
      NOTREACHED();
  }
}

::media::AudioCodec ToMediaAudioCodec(
    const chromecast::media::AudioCodec codec) {
  switch (codec) {
    case kAudioCodecUnknown:
      return ::media::AudioCodec::kUnknown;
    case kCodecAAC:
      return ::media::AudioCodec::kAAC;
    case kCodecMP3:
      return ::media::AudioCodec::kMP3;
    case kCodecPCM:
      return ::media::AudioCodec::kPCM;
    case kCodecPCM_S16BE:
      return ::media::AudioCodec::kPCM_S16BE;
    case kCodecVorbis:
      return ::media::AudioCodec::kVorbis;
    case kCodecOpus:
      return ::media::AudioCodec::kOpus;
    case kCodecFLAC:
      return ::media::AudioCodec::kFLAC;
    case kCodecEAC3:
      return ::media::AudioCodec::kEAC3;
    case kCodecAC3:
      return ::media::AudioCodec::kAC3;
    case kCodecMpegHAudio:
      return ::media::AudioCodec::kMpegHAudio;
    case kCodecDTS:
      return ::media::AudioCodec::kDTS;
    case kCodecDTSXP2:
      return ::media::AudioCodec::kDTSXP2;
    case kCodecDTSE:
      return ::media::AudioCodec::kDTSE;
    default:
      return ::media::AudioCodec::kUnknown;
  }
}

EncryptionScheme ToEncryptionScheme(::media::EncryptionScheme scheme) {
  switch (scheme) {
    case ::media::EncryptionScheme::kUnencrypted:
      return EncryptionScheme::kUnencrypted;
    case ::media::EncryptionScheme::kCenc:
      return EncryptionScheme::kAesCtr;
    case ::media::EncryptionScheme::kCbcs:
      return EncryptionScheme::kAesCbc;
    default:
      NOTREACHED();
  }
}

::media::EncryptionScheme ToMediaEncryptionScheme(EncryptionScheme scheme) {
  switch (scheme) {
    case EncryptionScheme::kUnencrypted:
      return ::media::EncryptionScheme::kUnencrypted;
    case EncryptionScheme::kAesCtr:
      return ::media::EncryptionScheme::kCenc;
    case EncryptionScheme::kAesCbc:
      return ::media::EncryptionScheme::kCbcs;
    default:
      NOTREACHED();
  }
}

}  // namespace

// static
ChannelLayout DecoderConfigAdapter::ToChannelLayout(
    ::media::ChannelLayout channel_layout) {
  switch (channel_layout) {
    case ::media::ChannelLayout::CHANNEL_LAYOUT_UNSUPPORTED:
      return ChannelLayout::UNSUPPORTED;
    case ::media::ChannelLayout::CHANNEL_LAYOUT_MONO:
      return ChannelLayout::MONO;
    case ::media::ChannelLayout::CHANNEL_LAYOUT_STEREO:
      return ChannelLayout::STEREO;
    case ::media::ChannelLayout::CHANNEL_LAYOUT_5_1:
    case ::media::ChannelLayout::CHANNEL_LAYOUT_5_1_BACK:
      return ChannelLayout::SURROUND_5_1;
    case ::media::ChannelLayout::CHANNEL_LAYOUT_BITSTREAM:
      return ChannelLayout::BITSTREAM;
    case ::media::ChannelLayout::CHANNEL_LAYOUT_DISCRETE:
      return ChannelLayout::DISCRETE;

    default:
      NOTREACHED();
  }
}

// static
::media::ChannelLayout DecoderConfigAdapter::ToMediaChannelLayout(
    ChannelLayout channel_layout) {
  switch (channel_layout) {
    case ChannelLayout::UNSUPPORTED:
      return ::media::ChannelLayout::CHANNEL_LAYOUT_UNSUPPORTED;
    case ChannelLayout::MONO:
      return ::media::ChannelLayout::CHANNEL_LAYOUT_MONO;
    case ChannelLayout::STEREO:
      return ::media::ChannelLayout::CHANNEL_LAYOUT_STEREO;
    case ChannelLayout::SURROUND_5_1:
      return ::media::ChannelLayout::CHANNEL_LAYOUT_5_1;
    case ChannelLayout::BITSTREAM:
      return ::media::ChannelLayout::CHANNEL_LAYOUT_BITSTREAM;
    case ChannelLayout::DISCRETE:
      return ::media::ChannelLayout::CHANNEL_LAYOUT_DISCRETE;

    default:
      NOTREACHED();
  }
}

// static
AudioConfig DecoderConfigAdapter::ToCastAudioConfig(
    StreamId id,
    const ::media::AudioDecoderConfig& config) {
  AudioConfig audio_config;
  if (!config.IsValidConfig()) {
    return audio_config;
  }

  audio_config.id = id;
  audio_config.codec = ToAudioCodec(config.codec());
  audio_config.sample_format = ToSampleFormat(config.sample_format());
  audio_config.bytes_per_channel = config.bytes_per_channel();
  audio_config.channel_layout = ToChannelLayout(config.channel_layout());
  audio_config.channel_number = config.channels();
  audio_config.samples_per_second = config.samples_per_second();
  audio_config.extra_data = config.extra_data();
  audio_config.encryption_scheme =
      ToEncryptionScheme(config.encryption_scheme());

#if BUILDFLAG(IS_ANDROID)
  // On Android, Chromium's mp4 parser adds extra data for AAC, but we don't
  // need this with CMA.
  if (audio_config.codec == kCodecAAC) {
    audio_config.extra_data.clear();
  }
#endif  // BUILDFLAG(IS_ANDROID)

  return audio_config;
}

// static
::media::AudioDecoderConfig DecoderConfigAdapter::ToMediaAudioDecoderConfig(
    const AudioConfig& config) {
  ::media::AudioDecoderConfig audio_decoder_config(
      ToMediaAudioCodec(config.codec),
      ToMediaSampleFormat(config.sample_format),
      ToMediaChannelLayout(config.channel_layout), config.samples_per_second,
      config.extra_data, ToMediaEncryptionScheme(config.encryption_scheme));
  if (config.channel_layout == ChannelLayout::DISCRETE) {
    audio_decoder_config.SetChannelsForDiscrete(config.channel_number);
  }
  return audio_decoder_config;
}

// static
#define STATIC_ASSERT_MATCHING_ENUM(chromium_name, chromecast_name)          \
  static_assert(static_cast<int>(::media::VideoColorSpace::chromium_name) == \
                    static_cast<int>(::chromecast::media::chromecast_name),  \
                "mismatching status enum values: " #chromium_name)

STATIC_ASSERT_MATCHING_ENUM(PrimaryID::BT709, PrimaryID::BT709);
STATIC_ASSERT_MATCHING_ENUM(PrimaryID::UNSPECIFIED, PrimaryID::UNSPECIFIED);
STATIC_ASSERT_MATCHING_ENUM(PrimaryID::BT470M, PrimaryID::BT470M);
STATIC_ASSERT_MATCHING_ENUM(PrimaryID::BT470BG, PrimaryID::BT470BG);
STATIC_ASSERT_MATCHING_ENUM(PrimaryID::SMPTE170M, PrimaryID::SMPTE170M);
STATIC_ASSERT_MATCHING_ENUM(PrimaryID::SMPTE240M, PrimaryID::SMPTE240M);
STATIC_ASSERT_MATCHING_ENUM(PrimaryID::FILM, PrimaryID::FILM);
STATIC_ASSERT_MATCHING_ENUM(PrimaryID::BT2020, PrimaryID::BT2020);
STATIC_ASSERT_MATCHING_ENUM(PrimaryID::SMPTEST428_1, PrimaryID::SMPTEST428_1);
STATIC_ASSERT_MATCHING_ENUM(PrimaryID::SMPTEST431_2, PrimaryID::SMPTEST431_2);
STATIC_ASSERT_MATCHING_ENUM(PrimaryID::SMPTEST432_1, PrimaryID::SMPTEST432_1);
STATIC_ASSERT_MATCHING_ENUM(PrimaryID::EBU_3213_E, PrimaryID::EBU_3213_E);

STATIC_ASSERT_MATCHING_ENUM(TransferID::BT709, TransferID::BT709);
STATIC_ASSERT_MATCHING_ENUM(TransferID::UNSPECIFIED, TransferID::UNSPECIFIED);
STATIC_ASSERT_MATCHING_ENUM(TransferID::GAMMA22, TransferID::GAMMA22);
STATIC_ASSERT_MATCHING_ENUM(TransferID::GAMMA28, TransferID::GAMMA28);
STATIC_ASSERT_MATCHING_ENUM(TransferID::SMPTE170M, TransferID::SMPTE170M);
STATIC_ASSERT_MATCHING_ENUM(TransferID::SMPTE240M, TransferID::SMPTE240M);
STATIC_ASSERT_MATCHING_ENUM(TransferID::LINEAR, TransferID::LINEAR);
STATIC_ASSERT_MATCHING_ENUM(TransferID::LOG, TransferID::LOG);
STATIC_ASSERT_MATCHING_ENUM(TransferID::LOG_SQRT, TransferID::LOG_SQRT);
STATIC_ASSERT_MATCHING_ENUM(TransferID::IEC61966_2_4, TransferID::IEC61966_2_4);
STATIC_ASSERT_MATCHING_ENUM(TransferID::BT1361_ECG, TransferID::BT1361_ECG);
STATIC_ASSERT_MATCHING_ENUM(TransferID::IEC61966_2_1, TransferID::IEC61966_2_1);
STATIC_ASSERT_MATCHING_ENUM(TransferID::BT2020_10, TransferID::BT2020_10);
STATIC_ASSERT_MATCHING_ENUM(TransferID::BT2020_12, TransferID::BT2020_12);
STATIC_ASSERT_MATCHING_ENUM(TransferID::SMPTEST2084, TransferID::SMPTEST2084);
STATIC_ASSERT_MATCHING_ENUM(TransferID::SMPTEST428_1, TransferID::SMPTEST428_1);
STATIC_ASSERT_MATCHING_ENUM(TransferID::ARIB_STD_B67, TransferID::ARIB_STD_B67);

STATIC_ASSERT_MATCHING_ENUM(MatrixID::RGB, MatrixID::RGB);
STATIC_ASSERT_MATCHING_ENUM(MatrixID::BT709, MatrixID::BT709);
STATIC_ASSERT_MATCHING_ENUM(MatrixID::UNSPECIFIED, MatrixID::UNSPECIFIED);
STATIC_ASSERT_MATCHING_ENUM(MatrixID::FCC, MatrixID::FCC);
STATIC_ASSERT_MATCHING_ENUM(MatrixID::BT470BG, MatrixID::BT470BG);
STATIC_ASSERT_MATCHING_ENUM(MatrixID::SMPTE170M, MatrixID::SMPTE170M);
STATIC_ASSERT_MATCHING_ENUM(MatrixID::SMPTE240M, MatrixID::SMPTE240M);
STATIC_ASSERT_MATCHING_ENUM(MatrixID::YCOCG, MatrixID::YCOCG);
STATIC_ASSERT_MATCHING_ENUM(MatrixID::BT2020_NCL, MatrixID::BT2020_NCL);
STATIC_ASSERT_MATCHING_ENUM(MatrixID::BT2020_CL, MatrixID::BT2020_CL);
STATIC_ASSERT_MATCHING_ENUM(MatrixID::YDZDX, MatrixID::YDZDX);

#define STATIC_ASSERT_MATCHING_ENUM2(chromium_name, chromecast_name)        \
  static_assert(static_cast<int>(::gfx::ColorSpace::chromium_name) ==       \
                    static_cast<int>(::chromecast::media::chromecast_name), \
                "mismatching status enum values: " #chromium_name)

STATIC_ASSERT_MATCHING_ENUM2(RangeID::INVALID, RangeID::INVALID);
STATIC_ASSERT_MATCHING_ENUM2(RangeID::LIMITED, RangeID::LIMITED);
STATIC_ASSERT_MATCHING_ENUM2(RangeID::FULL, RangeID::FULL);
STATIC_ASSERT_MATCHING_ENUM2(RangeID::DERIVED, RangeID::DERIVED);

VideoConfig DecoderConfigAdapter::ToCastVideoConfig(
    StreamId id,
    const ::media::VideoDecoderConfig& config) {
  VideoConfig video_config;
  if (!config.IsValidConfig()) {
    return video_config;
  }

  video_config.id = id;
  video_config.codec = ToCastVideoCodec(config.codec(), config.profile());
  video_config.profile = ToCastVideoProfile(config.profile());
  video_config.codec_profile_level = config.level();
  video_config.extra_data = config.extra_data();
  video_config.encryption_scheme =
      ToEncryptionScheme(config.encryption_scheme());

  video_config.primaries =
      static_cast<PrimaryID>(config.color_space_info().primaries);
  video_config.transfer =
      static_cast<TransferID>(config.color_space_info().transfer);
  video_config.matrix = static_cast<MatrixID>(config.color_space_info().matrix);
  video_config.range = static_cast<RangeID>(config.color_space_info().range);

  std::optional<::gfx::HDRMetadata> hdr_metadata = config.hdr_metadata();
  if (hdr_metadata) {
    video_config.have_hdr_metadata = true;

    if (const auto& cta_861_3 = hdr_metadata->cta_861_3) {
      video_config.hdr_metadata.max_content_light_level =
          cta_861_3->max_content_light_level;
      video_config.hdr_metadata.max_frame_average_light_level =
          cta_861_3->max_frame_average_light_level;
    }

    if (const auto& mm1 = hdr_metadata->smpte_st_2086) {
      auto& mm2 = video_config.hdr_metadata.color_volume_metadata;
      mm2.primary_r_chromaticity_x = mm1->primaries.fRX;
      mm2.primary_r_chromaticity_y = mm1->primaries.fRY;
      mm2.primary_g_chromaticity_x = mm1->primaries.fGX;
      mm2.primary_g_chromaticity_y = mm1->primaries.fGY;
      mm2.primary_b_chromaticity_x = mm1->primaries.fBX;
      mm2.primary_b_chromaticity_y = mm1->primaries.fBY;
      mm2.white_point_chromaticity_x = mm1->primaries.fWX;
      mm2.white_point_chromaticity_y = mm1->primaries.fWY;
      mm2.luminance_max = mm1->luminance_max;
      mm2.luminance_min = mm1->luminance_min;
    }
  }

  const gfx::Size aspect_ratio =
      config.aspect_ratio().GetNaturalSize(config.visible_rect());
  video_config.width = aspect_ratio.width();
  video_config.height = aspect_ratio.height();

  return video_config;
}

}  // namespace media
}  // namespace chromecast
