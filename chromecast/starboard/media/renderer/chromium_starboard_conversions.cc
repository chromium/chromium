// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/renderer/chromium_starboard_conversions.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "chromecast/starboard/media/media/mime_utils.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "media/base/audio_codecs.h"
#include "media/base/channel_layout.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "third_party/abseil-cpp/absl/container/node_hash_set.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

namespace chromecast {
namespace media {

namespace {

using ::media::VideoColorSpace;

// Registers the given MIME type, if it has not already been registered. Returns
// a c string of the MIME type, which is guaranteed to point to valid data for
// the duration of the program.
const char* RegisterMimeType(std::string mime) {
  if (mime.empty()) {
    return "";
  }

  static base::NoDestructor<base::Lock> lock;
  // A node_hash_set is used here because we require key ptr stability (so that
  // c strings remain valid).
  static base::NoDestructor<absl::node_hash_set<std::string>> registry
      GUARDED_BY(*lock);

  base::AutoLock autlock(*lock);
  auto it_and_inserted = registry->insert(std::move(mime));
  return it_and_inserted.first->c_str();
}

// Converts a chromium codec to a starboard codec, returning nullopt if the
// codec does not exist in starboard.
//
// `profile` is necessary for differentiating Dolby Vision codecs.
std::optional<StarboardVideoCodec> ToSbVideoCodec(
    ::media::VideoCodec codec,
    ::media::VideoCodecProfile profile) {
  switch (codec) {
    case ::media::VideoCodec::kH264:
      return StarboardVideoCodec::kStarboardVideoCodecH264;
    case ::media::VideoCodec::kMPEG2:
      return StarboardVideoCodec::kStarboardVideoCodecMpeg2;
    case ::media::VideoCodec::kTheora:
      return StarboardVideoCodec::kStarboardVideoCodecTheora;
    case ::media::VideoCodec::kVP8:
      return StarboardVideoCodec::kStarboardVideoCodecVp8;
    case ::media::VideoCodec::kVP9:
      return StarboardVideoCodec::kStarboardVideoCodecVp9;
    case ::media::VideoCodec::kHEVC:
      return StarboardVideoCodec::kStarboardVideoCodecH265;
    case ::media::VideoCodec::kAV1:
      return StarboardVideoCodec::kStarboardVideoCodecAv1;
    case ::media::VideoCodec::kVC1:
      return StarboardVideoCodec::kStarboardVideoCodecVc1;
    case ::media::VideoCodec::kDolbyVision:
      switch (profile) {
        // This logic was copied from
        // https://source.chromium.org/chromium/chromium/src/+/main:chromecast/media/base/media_codec_support.cc;l=63;drc=586d9e059d27bfbe85c8df737882821e7b68929d
        case ::media::VideoCodecProfile::DOLBYVISION_PROFILE5:
        case ::media::VideoCodecProfile::DOLBYVISION_PROFILE7:
        case ::media::VideoCodecProfile::DOLBYVISION_PROFILE8:
          return StarboardVideoCodec::kStarboardVideoCodecH265;
        default:
          // We only support Dolby Vision for HEVC currently. The H264 profiles
          // (DOLBYVISION_PROFILE0 and DOLBYVISION_PROFILE9) are considered
          // unsupported.
          LOG(INFO) << "Unsupported Dolby Vision profile=" << profile;
          return std::nullopt;
      }
    default:
      LOG(ERROR) << "Unsupported video codec: " << codec;
      return std::nullopt;
  }
}

// Populates HDR fields of `out_color_metadata` based on `hdr_metadata`.
void PopulateHdrMetadata(const gfx::HDRMetadata& hdr_metadata,
                         StarboardColorMetadata& out_color_metadata) {
  if (hdr_metadata.cta_861_3) {
    out_color_metadata.max_cll =
        hdr_metadata.cta_861_3->max_content_light_level;
    out_color_metadata.max_fall =
        hdr_metadata.cta_861_3->max_frame_average_light_level;
  } else {
    LOG(INFO) << "HDR metadata is missing cta_861_3 info.";
  }

  if (hdr_metadata.smpte_st_2086) {
    const auto& color_volume_metadata = hdr_metadata.smpte_st_2086->primaries;

    StarboardMediaMasteringMetadata& mastering_metadata =
        out_color_metadata.mastering_metadata;
    mastering_metadata.primary_r_chromaticity_x = color_volume_metadata.fRX;
    mastering_metadata.primary_r_chromaticity_y = color_volume_metadata.fRY;
    mastering_metadata.primary_g_chromaticity_x = color_volume_metadata.fGX;
    mastering_metadata.primary_g_chromaticity_y = color_volume_metadata.fGY;
    mastering_metadata.primary_b_chromaticity_x = color_volume_metadata.fBX;
    mastering_metadata.primary_b_chromaticity_y = color_volume_metadata.fBY;
    mastering_metadata.white_point_chromaticity_x = color_volume_metadata.fWX;
    mastering_metadata.white_point_chromaticity_y = color_volume_metadata.fWY;
    mastering_metadata.luminance_max =
        hdr_metadata.smpte_st_2086->luminance_max;
    mastering_metadata.luminance_min =
        hdr_metadata.smpte_st_2086->luminance_min;
  } else {
    LOG(INFO) << "HDR metadata is missing smpte_st_2086 info.";
  }
}

// Converts chromium color metadata to starboard color metadata, returning
// nullopt if the chromium color metadata cannot be converted.
std::optional<StarboardColorMetadata> ToSbColorMetadata(
    const std::optional<gfx::HDRMetadata>& hdr_metadata,
    const ::media::VideoColorSpace& color_space) {
  StarboardColorMetadata color_metadata = {};
  // bits_per_channel and the chroma_*/cb_* fields below need to be derived from
  // the MIME string. See crbug.com/230915942 for more info.
  // Unfortunately, it doesn't look like MIME type is exposed to cast. Note that
  // in Cobalt, these fields are all currently hard-coded to zero (in
  // third_party/chromium/media/base/starboard_utils.cc). I don't think they're
  // necessary for cast either, since cast doesn't seem to populate this info
  // anywhere.

  // 0 translates to "unknown".
  color_metadata.bits_per_channel = 0;
  color_metadata.chroma_subsampling_horizontal = 0;
  color_metadata.chroma_subsampling_vertical = 0;
  color_metadata.cb_subsampling_horizontal = 0;
  color_metadata.cb_subsampling_vertical = 0;
  color_metadata.chroma_siting_horizontal = 0;
  color_metadata.chroma_siting_vertical = 0;

  if (hdr_metadata) {
    LOG(INFO) << "Video config has HDR metadata.";
    PopulateHdrMetadata(*hdr_metadata, color_metadata);
  } else {
    LOG(INFO) << "Video config has no HDR metadata.";
    color_metadata.max_cll = 0;
    color_metadata.max_fall = 0;
  }

  switch (color_space.primaries) {
    case VideoColorSpace::PrimaryID::INVALID:
    case VideoColorSpace::PrimaryID::BT709:
    case VideoColorSpace::PrimaryID::UNSPECIFIED:
    case VideoColorSpace::PrimaryID::BT470M:
    case VideoColorSpace::PrimaryID::BT470BG:
    case VideoColorSpace::PrimaryID::SMPTE170M:
    case VideoColorSpace::PrimaryID::SMPTE240M:
    case VideoColorSpace::PrimaryID::FILM:
    case VideoColorSpace::PrimaryID::BT2020:
    case VideoColorSpace::PrimaryID::SMPTEST428_1:
    case VideoColorSpace::PrimaryID::SMPTEST431_2:
    case VideoColorSpace::PrimaryID::SMPTEST432_1:
      color_metadata.primaries = static_cast<int>(color_space.primaries);
      break;
    default:
      LOG(ERROR) << "Unsupported color space primaries: "
                 << static_cast<int>(color_space.primaries);
      return std::nullopt;
  }

  switch (color_space.transfer) {
    case VideoColorSpace::TransferID::INVALID:
    case VideoColorSpace::TransferID::BT709:
    case VideoColorSpace::TransferID::UNSPECIFIED:
    case VideoColorSpace::TransferID::GAMMA22:
    case VideoColorSpace::TransferID::GAMMA28:
    case VideoColorSpace::TransferID::SMPTE170M:
    case VideoColorSpace::TransferID::SMPTE240M:
    case VideoColorSpace::TransferID::LINEAR:
    case VideoColorSpace::TransferID::LOG:
    case VideoColorSpace::TransferID::LOG_SQRT:
    case VideoColorSpace::TransferID::IEC61966_2_4:
    case VideoColorSpace::TransferID::BT1361_ECG:
    case VideoColorSpace::TransferID::IEC61966_2_1:
    case VideoColorSpace::TransferID::BT2020_10:
    case VideoColorSpace::TransferID::BT2020_12:
    case VideoColorSpace::TransferID::SMPTEST2084:
    case VideoColorSpace::TransferID::SMPTEST428_1:
      color_metadata.transfer = static_cast<int>(color_space.transfer);
      break;
    default:
      LOG(ERROR) << "Unsupported color space transfer: "
                 << static_cast<int>(color_space.transfer);
      return std::nullopt;
  }

  switch (color_space.matrix) {
    case VideoColorSpace::MatrixID::RGB:
    case VideoColorSpace::MatrixID::BT709:
    case VideoColorSpace::MatrixID::UNSPECIFIED:
    case VideoColorSpace::MatrixID::FCC:
    case VideoColorSpace::MatrixID::BT470BG:
    case VideoColorSpace::MatrixID::SMPTE170M:
    case VideoColorSpace::MatrixID::SMPTE240M:
    case VideoColorSpace::MatrixID::YCOCG:
    case VideoColorSpace::MatrixID::BT2020_NCL:
    case VideoColorSpace::MatrixID::BT2020_CL:
    case VideoColorSpace::MatrixID::YDZDX:
    case VideoColorSpace::MatrixID::INVALID:
      color_metadata.matrix = static_cast<int>(color_space.matrix);
      break;
    default:
      LOG(ERROR) << "Unsupported color space matrix: "
                 << static_cast<int>(color_space.matrix);
      return std::nullopt;
  }

  switch (color_space.range) {
    case gfx::ColorSpace::RangeID::INVALID:
    case gfx::ColorSpace::RangeID::LIMITED:
    case gfx::ColorSpace::RangeID::FULL:
    case gfx::ColorSpace::RangeID::DERIVED:
      color_metadata.range = static_cast<int>(color_space.range);
      break;
    default:
      LOG(ERROR) << "Unsupported color space range: "
                 << static_cast<int>(color_space.range);
      return std::nullopt;
  }

  // color_space.primaries (::media::VideoColorSpace::PrimaryID)
  // does not support any value equivalent to Starboard's
  // kSbMediaPrimaryIdCustom. Thus, we don't need to populate
  // custom_primary_matrix. Just zero it, in case something reads from it.
  return color_metadata;
}

}  // namespace

std::optional<StarboardAudioSampleInfo> ToStarboardAudioSampleInfo(
    const ::media::AudioDecoderConfig& in_config) {
  StarboardAudioSampleInfo out_config = {};

  switch (in_config.codec()) {
    case ::media::AudioCodec::kAAC:
      out_config.codec = StarboardAudioCodec::kStarboardAudioCodecAac;
      break;
    case ::media::AudioCodec::kMP3:
      out_config.codec = StarboardAudioCodec::kStarboardAudioCodecMp3;
      break;
    case ::media::AudioCodec::kVorbis:
      out_config.codec = StarboardAudioCodec::kStarboardAudioCodecVorbis;
      break;
    case ::media::AudioCodec::kFLAC:
      out_config.codec = StarboardAudioCodec::kStarboardAudioCodecFlac;
      break;
    case ::media::AudioCodec::kPCM:
    case ::media::AudioCodec::kPCM_S16BE:
    case ::media::AudioCodec::kPCM_S24BE:
      out_config.codec = StarboardAudioCodec::kStarboardAudioCodecPcm;
      break;
    case ::media::AudioCodec::kOpus:
      out_config.codec = StarboardAudioCodec::kStarboardAudioCodecOpus;
      break;
    case ::media::AudioCodec::kEAC3:
      out_config.codec = StarboardAudioCodec::kStarboardAudioCodecEac3;
      break;
    case ::media::AudioCodec::kAC3:
      out_config.codec = StarboardAudioCodec::kStarboardAudioCodecAc3;
      break;
    default:
      LOG(ERROR) << "Unsupported audio codec: " << in_config.codec();
      return std::nullopt;
  }

  out_config.mime = RegisterMimeType(GetMimeType(in_config.codec()));
  out_config.format_tag = 0;
  out_config.number_of_channels = in_config.channels();
  out_config.samples_per_second = in_config.samples_per_second();

  // Based on starboard_utils.cc (MediaAudioConfigToSbMediaAudioSampleInfo) in
  // the cobalt codebase, bits_per_sample  does not take into account the number
  // of channels.
  if (out_config.codec == StarboardAudioCodec::kStarboardAudioCodecPcm) {
    // TODO(antoniori): handle resampling to other formats. Currently we only
    // support S16.
    out_config.bits_per_sample = 16;
  } else {
    // For starboard, "bits per sample" does not factor in channel count.
    out_config.bits_per_sample = in_config.bytes_per_channel() * 8;
  }
  out_config.average_bytes_per_second = out_config.number_of_channels *
                                        out_config.samples_per_second *
                                        out_config.bits_per_sample / 8;
  out_config.block_alignment = 4;
  out_config.audio_specific_config_size = in_config.extra_data().size();
  out_config.audio_specific_config = in_config.extra_data().data();

  return out_config;
}

std::optional<StarboardVideoSampleInfo> ToStarboardVideoSampleInfo(
    const ::media::VideoDecoderConfig& in_config) {
  StarboardVideoSampleInfo out_config = {};

  std::optional<StarboardVideoCodec> sb_codec =
      ToSbVideoCodec(in_config.codec(), in_config.profile());
  if (!sb_codec.has_value()) {
    return std::nullopt;
  }

  out_config.codec = *sb_codec;
  out_config.mime = RegisterMimeType(
      GetMimeType(in_config.codec(), in_config.profile(), in_config.level()));
  // Specify that the max capabilities are not known.
  out_config.max_video_capabilities = "";
  // This needs to be set on a per-sample basis later.
  out_config.is_key_frame = false;

  const gfx::Size aspect_ratio = in_config.coded_size();
  out_config.frame_width = aspect_ratio.width();
  out_config.frame_height = aspect_ratio.height();

  std::optional<StarboardColorMetadata> sb_color_metadata =
      ToSbColorMetadata(in_config.hdr_metadata(), in_config.color_space_info());
  if (!sb_color_metadata.has_value()) {
    return std::nullopt;
  }
  out_config.color_metadata = *std::move(sb_color_metadata);

  return out_config;
}

}  // namespace media
}  // namespace chromecast
