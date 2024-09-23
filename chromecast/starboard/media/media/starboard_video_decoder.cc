// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "starboard_video_decoder.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/starboard/media/media/drm_util.h"
#include "chromecast/starboard/media/media/mime_utils.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"

namespace chromecast {
namespace media {

using BufferStatus = ::chromecast::media::MediaPipelineBackend::BufferStatus;

static StarboardVideoCodec VideoCodecToStarboardCodec(VideoCodec codec) {
  switch (codec) {
    case kCodecH264:
      return kStarboardVideoCodecH264;
    case kCodecVC1:
      return kStarboardVideoCodecVc1;
    case kCodecMPEG2:
      return kStarboardVideoCodecMpeg2;
    case kCodecTheora:
      return kStarboardVideoCodecTheora;
    case kCodecVP8:
      return kStarboardVideoCodecVp8;
    case kCodecVP9:
      return kStarboardVideoCodecVp9;
    case kCodecHEVC:
    case kCodecDolbyVisionHEVC:
      return kStarboardVideoCodecH265;
    case kCodecAV1:
      return kStarboardVideoCodecAv1;
    case kCodecDolbyVisionH264:
    case kCodecMPEG4:
    case kVideoCodecUnknown:
    default:
      LOG(ERROR) << "Unsupported video codec: " << codec;
      return kStarboardVideoCodecNone;
  }
}

// Converts a cast VideoConfig to a StarboardVideoSampleInfo. MIME type is not
// properly set, since it stores a c string pointing to data that could go out
// of scope. Instead, it is hardcoded to the empty string.
static StarboardVideoSampleInfo ToVideoSampleInfo(const VideoConfig& config) {
  StarboardVideoSampleInfo sample_info = {};

  sample_info.codec = VideoCodecToStarboardCodec(config.codec);
  sample_info.mime = "";
  // Unknown maximum capabilities. For adaptive playback, I don't think we can
  // guarantee the max video quality that the MSP will send.
  sample_info.max_video_capabilities = "";
  // We do not have info on whether a frame is actually a keyframe, so this is
  // hardcoded to true for now. We should confirm that starboard implementations
  // do not rely on this field, or submit a CL like crrev.com/c/4348024 to
  // properly populate the field.
  sample_info.is_key_frame = true;
  sample_info.frame_width = config.width;
  sample_info.frame_height = config.height;

  StarboardColorMetadata& color_metadata = sample_info.color_metadata;
  // bits_per_channel and the chroma_*/cb_* fields below need to be derived from
  // the MIME string. See b/230915942 for more info.
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

  if (config.have_hdr_metadata) {
    LOG(INFO) << "Video config has HDR metadata.";

    color_metadata.max_cll = config.hdr_metadata.max_content_light_level;
    color_metadata.max_fall = config.hdr_metadata.max_frame_average_light_level;

    const auto& color_volume_metadata =
        config.hdr_metadata.color_volume_metadata;
    StarboardMediaMasteringMetadata& mastering_metadata =
        color_metadata.mastering_metadata;

    mastering_metadata.primary_r_chromaticity_x =
        color_volume_metadata.primary_r_chromaticity_x;
    mastering_metadata.primary_r_chromaticity_y =
        color_volume_metadata.primary_r_chromaticity_y;
    mastering_metadata.primary_g_chromaticity_x =
        color_volume_metadata.primary_g_chromaticity_x;
    mastering_metadata.primary_g_chromaticity_y =
        color_volume_metadata.primary_g_chromaticity_y;
    mastering_metadata.primary_b_chromaticity_x =
        color_volume_metadata.primary_b_chromaticity_x;
    mastering_metadata.primary_b_chromaticity_y =
        color_volume_metadata.primary_b_chromaticity_y;
    mastering_metadata.white_point_chromaticity_x =
        color_volume_metadata.white_point_chromaticity_x;
    mastering_metadata.white_point_chromaticity_y =
        color_volume_metadata.white_point_chromaticity_y;
    mastering_metadata.luminance_max = color_volume_metadata.luminance_max;
    mastering_metadata.luminance_min = color_volume_metadata.luminance_min;
  } else {
    color_metadata.max_cll = 0;
    color_metadata.max_fall = 0;
  }

  // config.primaries (::chromecast::media::PrimaryID) does not support any
  // value equivalent to kSbMediaPrimaryIdCustom. Thus, we don't need to
  // populate custom_primary_matrix.
  color_metadata.primaries = static_cast<int>(config.primaries);
  color_metadata.transfer = static_cast<int>(config.transfer);
  color_metadata.matrix = static_cast<int>(config.matrix);
  color_metadata.range = static_cast<int>(config.range);

  return sample_info;
}

StarboardVideoDecoder::StarboardVideoDecoder(StarboardApiWrapper* starboard)
    : StarboardDecoder(starboard, kStarboardMediaTypeVideo) {}

StarboardVideoDecoder::~StarboardVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void StarboardVideoDecoder::InitializeInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

const std::optional<StarboardVideoSampleInfo>&
StarboardVideoDecoder::GetVideoSampleInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return video_sample_info_;
}

bool StarboardVideoDecoder::SetConfig(const VideoConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  resolution_changed_ =
      config.width != config_.width || config.height != config_.height;
  if (resolution_changed_) {
    LOG(INFO) << "Video resolution changed from (" << config_.width << ", "
              << config_.height << ") to (" << config.width << ", "
              << config.height << ")";
  }

  // Note: we do not call delegate->OnVideoResolutionChanged in this function,
  // because the VideoPipelineImpl (the delegate) may not be in the kPlaying
  // state. In that case, it ignores resolution changes. So we wait until
  // PushBuffer; at that point, it must be in the kPlaying state.

  config_ = config;
  video_sample_info_ = ToVideoSampleInfo(config_);

  codec_mime_ =
      GetMimeType(config.codec, config.profile, config.codec_profile_level);
  video_sample_info_->mime = codec_mime_.c_str();

  return IsValidConfig(config_);
}

BufferStatus StarboardVideoDecoder::PushBuffer(CastDecoderBuffer* buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer);

  // At this point the VideoPipelineImpl (the delegate) should be in the
  // kPlaying state, so it is safe to update the resolution.
  MediaPipelineBackend::Decoder::Delegate* const delegate = GetDelegate();
  if (resolution_changed_ && delegate) {
    delegate->OnVideoResolutionChanged(
        chromecast::Size(config_.width, config_.height));
    resolution_changed_ = false;
  }

  if (buffer->end_of_stream()) {
    return PushEndOfStream();
  }

  DCHECK(video_sample_info_);

  const size_t copy_size = buffer->data_size();
  std::unique_ptr<uint8_t[]> data_copy(new uint8_t[copy_size]);
  uint8_t* const copy_addr = data_copy.get();
  memcpy(copy_addr, buffer->data(), copy_size);

  StarboardSampleInfo sample = {};
  sample.type = kStarboardMediaTypeVideo;
  sample.timestamp = buffer->timestamp();
  sample.side_data = nullptr;
  sample.side_data_count = 0;
  sample.video_sample_info = *video_sample_info_;

  decoded_bytes_ += copy_size;

  return PushBufferInternal(std::move(sample), GetDrmInfo(*buffer),
                            std::move(data_copy), copy_size);
}

void StarboardVideoDecoder::GetStatistics(Statistics* statistics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!statistics) {
    return;
  }

  StarboardPlayerInfo player_info = {};
  GetStarboardApi().GetPlayerInfo(GetPlayer(), &player_info);

  statistics->decoded_bytes = decoded_bytes_;
  statistics->decoded_frames = player_info.total_video_frames;
  statistics->dropped_frames = player_info.dropped_video_frames;
}

void StarboardVideoDecoder::SetDelegate(Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetDecoderDelegate(delegate);
}

}  // namespace media
}  // namespace chromecast
