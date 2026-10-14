// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "starboard_video_decoder.h"

#include <algorithm>
#include <array>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/starboard/media/media/drm_util.h"
#include "chromecast/starboard/media/media/mime_utils.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "media/parsers/h264_parser.h"
#include "media/parsers/vp9_parser.h"

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
  if (IsInitialized() &&
      std::exchange(preserve_in_band_resolution_on_reinit_, false)) {
    has_pending_pre_init_push_ = false;
    return;
  }
  if (preserve_in_band_resolution_on_reinit_) {
    return;
  }
  if (std::exchange(has_pending_pre_init_push_, false) &&
      HasPendingFirstPush()) {
    return;
  }
  if (video_sample_info_ &&
      (config_.width != container_width_ ||
       config_.height != container_height_ ||
       video_sample_info_->frame_width != container_width_ ||
       video_sample_info_->frame_height != container_height_)) {
    config_.width = container_width_;
    config_.height = container_height_;
    video_sample_info_->frame_width = container_width_;
    video_sample_info_->frame_height = container_height_;
    resolution_changed_ = true;
  }
}

void StarboardVideoDecoder::StopInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  preserve_in_band_resolution_on_reinit_ = false;
}

const std::optional<StarboardVideoSampleInfo>&
StarboardVideoDecoder::GetVideoSampleInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return video_sample_info_;
}

void StarboardVideoDecoder::PrepareForPlayerRecreation(void* player) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (player) {
    StarboardPlayerInfo player_info = {};
    GetStarboardApi().GetPlayerInfo(player, &player_info);
    accumulated_decoded_frames_ += player_info.total_video_frames;
    accumulated_dropped_frames_ += player_info.dropped_video_frames;
  }
  RequeueInFlightWrite();
  ResetPlayerForRecreation();
  // Preserve any in-band resolution parsed on the previous SbPlayer when
  // Initialize(new_player) calls InitializeInternal().
  preserve_in_band_resolution_on_reinit_ = true;
}

std::optional<EncryptionScheme> StarboardVideoDecoder::GetEncryptionScheme() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (video_sample_info_.has_value()) {
    // The config is populated when video_sample_info_ is populated.
    return config_.encryption_scheme;
  }
  return std::nullopt;
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
  container_width_ = config.width;
  container_height_ = config.height;
  has_pending_pre_init_push_ = false;
  preserve_in_band_resolution_on_reinit_ = false;
  video_sample_info_ = ToVideoSampleInfo(config_);

  codec_mime_ =
      GetMimeType(config.codec, config.profile, config.codec_profile_level);
  video_sample_info_->mime = codec_mime_.c_str();

  return IsValidConfig(config_);
}

BufferStatus StarboardVideoDecoder::PushBuffer(CastDecoderBuffer* buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer);

  if (!DrmInfoWrapper::VerifySubsamplesMatchSize(*buffer)) {
    return BufferStatus::kBufferFailed;
  }

  if (!IsInitialized() && (!has_pending_pre_init_push_ ||
                           !HasPendingFirstPush() || buffer->end_of_stream())) {
    has_pending_pre_init_push_ = false;
    InitializeInternal();
  }
  if (!buffer->end_of_stream()) {
    has_pending_pre_init_push_ = !IsInitialized();
    CheckForInBandResolutionChange(*buffer);
  }

  // At this point the VideoPipelineImpl (the delegate) should be in the
  // kPlaying state, so it is safe to update the resolution.
  MediaPipelineBackend::Decoder::Delegate* const delegate = GetDelegate();
  if (std::exchange(resolution_changed_, false) && delegate) {
    delegate->OnVideoResolutionChanged(
        chromecast::Size(config_.width, config_.height));
  }

  if (buffer->end_of_stream()) {
    return PushEndOfStream();
  }

  DCHECK(video_sample_info_);

  // SAFETY: This is not safe, but is necessary since the CastDecoderBuffer API
  // only exposes its data as a pointer + size.
  //
  // TODO(crbug.com/323610278): once CMA is deprecated, we will not need this
  // class.
  base::HeapArray<uint8_t> data_copy =
      base::HeapArray<uint8_t>::CopiedFrom(UNSAFE_BUFFERS(
          base::span<const uint8_t>(buffer->data(), buffer->data_size())));

  StarboardSampleInfo sample = {};
  sample.type = kStarboardMediaTypeVideo;
  sample.timestamp = buffer->timestamp();
  sample.side_data = base::span<const StarboardSampleSideData>();
  sample.video_sample_info = *video_sample_info_;

  decoded_bytes_ += data_copy.size();

  return PushBufferInternal(std::move(sample), DrmInfoWrapper::Create(*buffer),
                            std::move(data_copy));
}

void StarboardVideoDecoder::GetStatistics(Statistics* statistics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!statistics) {
    return;
  }

  StarboardPlayerInfo player_info = {};
  auto* player = GetPlayer();
  if (player) {
    GetStarboardApi().GetPlayerInfo(player, &player_info);
  }

  statistics->decoded_bytes = decoded_bytes_;
  statistics->decoded_frames =
      accumulated_decoded_frames_ + player_info.total_video_frames;
  statistics->dropped_frames =
      accumulated_dropped_frames_ + player_info.dropped_video_frames;
}

void StarboardVideoDecoder::SetDelegate(Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetDecoderDelegate(delegate);
}

void StarboardVideoDecoder::CheckForInBandResolutionChange(
    const CastDecoderBuffer& buffer) {
  if (buffer.end_of_stream() || !buffer.data() || buffer.data_size() == 0 ||
      buffer.decrypt_config()) {
    return;
  }
  // SAFETY: CastDecoderBuffer provides buffer.data() and verified data_size().
  auto data = UNSAFE_BUFFERS(base::span(buffer.data(), buffer.data_size()));
  if (config_.codec == kCodecH264) {
    ::media::H264Parser parser;
    parser.SetStream(data.first(std::min<size_t>(data.size(), 1024)));
    ::media::H264NALU nalu;
    int sps_id = -1;
    while (parser.AdvanceToNextNALU(&nalu) == ::media::H264Parser::kOk) {
      if (nalu.nal_unit_type == ::media::H264NALU::kSPS &&
          parser.ParseSPS(&sps_id) == ::media::H264Parser::kOk) {
        if (const ::media::H264SPS* sps = parser.GetSPS(sps_id)) {
          if (auto rect = sps->GetVisibleRect(); rect && !rect->IsEmpty()) {
            UpdateResolution(rect->width(), rect->height(), rect->width(),
                             rect->height(), "H.264");
          }
        }
      } else if (nalu.nal_unit_type >= ::media::H264NALU::kNonIDRSlice &&
                 nalu.nal_unit_type <= ::media::H264NALU::kIDRSlice) {
        break;
      }
    }
  } else if (config_.codec == kCodecVP9) {
    ::media::Vp9Parser parser;
    parser.SetStream(data, nullptr);
    ::media::Vp9FrameHeader fhdr;
    gfx::Size allocate_size;
    for (::media::Vp9Parser::Result res;
         (res = parser.ParseNextFrame(&fhdr, &allocate_size, nullptr)) !=
         ::media::Vp9Parser::kEOStream;) {
      if (res == ::media::Vp9Parser::kOk && fhdr.IsKeyframe()) {
        const int w =
            fhdr.render_width > 0 ? fhdr.render_width : fhdr.frame_width;
        const int h =
            fhdr.render_height > 0 ? fhdr.render_height : fhdr.frame_height;
        UpdateResolution(w, h, fhdr.frame_width, fhdr.frame_height, "VP9");
      }
    }
  } else if (config_.codec == kCodecVP8 && data.size() >= 10 &&
             (data[0] & 0x01) == 0 && data[3] == 0x9d && data[4] == 0x01 &&
             data[5] == 0x2a) {
    constexpr std::array<int, 4> kNum = {1, 5, 5, 2}, kDen = {1, 4, 3, 1};
    const uint16_t w = base::U16FromLittleEndian(data.subspan<6, 2>());
    const uint16_t h = base::U16FromLittleEndian(data.subspan<8, 2>());
    const int coded_w = w & 0x3fff;
    const int coded_h = h & 0x3fff;
    const int width = (coded_w * kNum[w >> 14]) / kDen[w >> 14];
    const int height = (coded_h * kNum[h >> 14]) / kDen[h >> 14];
    UpdateResolution(width, height, coded_w, coded_h, "VP8");
  }
}

void StarboardVideoDecoder::UpdateResolution(int display_width,
                                             int display_height,
                                             int coded_width,
                                             int coded_height,
                                             const char* codec_name) {
  if (display_width > 0 && display_height > 0 &&
      (display_width != config_.width || display_height != config_.height)) {
    LOG(INFO) << "Detected in-band " << codec_name
              << " resolution change from (" << config_.width << ", "
              << config_.height << ") to (" << display_width << ", "
              << display_height << ")";
    config_.width = display_width;
    config_.height = display_height;
    resolution_changed_ = true;
  }
  if (coded_width > 0 && coded_height > 0 && video_sample_info_) {
    video_sample_info_->frame_width = coded_width;
    video_sample_info_->frame_height = coded_height;
  }
}

}  // namespace media
}  // namespace chromecast
