// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines APIs that are specific to Starboard 14. GetStarboardApiWrapper() is
// the public API; everything else is an implementation detail.
#include <starboard/drm.h>
#include <starboard/media.h>
#include <starboard/player.h>

#include <memory>
#include <vector>

#include "base/logging.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper_base.h"

namespace chromecast {
namespace media {

namespace {

// A concrete implementation of StarboardApiWrapper for Starboard version 14.
class StarboardApiWrapper14 : public StarboardApiWrapperBase {
 public:
  StarboardApiWrapper14() = default;
  ~StarboardApiWrapper14() override = default;

  // StarboardApiWrapper impl:
  void SeekTo(void* player, int64_t time, int seek_ticket) override {
    SbPlayerSeek2(static_cast<SbPlayer>(player), time, seek_ticket);
  }

  void GetPlayerInfo(void* player, StarboardPlayerInfo* player_info) override {
    SbPlayerInfo2 sb_player_info = {};
    SbPlayerGetInfo2(static_cast<SbPlayer>(player), &sb_player_info);

    player_info->current_media_timestamp_micros =
        sb_player_info.current_media_timestamp;
    player_info->duration_micros = sb_player_info.duration;
    player_info->start_date = sb_player_info.start_date;
    player_info->frame_width = sb_player_info.frame_width;
    player_info->frame_height = sb_player_info.frame_height;
    player_info->is_paused = sb_player_info.is_paused;
    player_info->volume = sb_player_info.volume;
    player_info->total_video_frames = sb_player_info.total_video_frames;
    player_info->dropped_video_frames = sb_player_info.dropped_video_frames;
    player_info->corrupted_video_frames = sb_player_info.corrupted_video_frames;
    player_info->playback_rate = sb_player_info.playback_rate;
  }

 private:
  // StarboardApiWrapperBase impl:
  SbPlayerCreationParam ToSbPlayerCreationParam(
      const StarboardPlayerCreationParam& in_param,
      void* drm_system) override {
    SbPlayerCreationParam out_param = {};

    out_param.audio_sample_info =
        ToSbMediaAudioSampleInfo(in_param.audio_sample_info);
    out_param.video_sample_info =
        ToSbMediaVideoSampleInfo(in_param.video_sample_info);
    out_param.output_mode =
        static_cast<SbPlayerOutputMode>(in_param.output_mode);

    if (drm_system) {
      LOG(INFO) << "Using an SbDrmSystem for decryption.";
      out_param.drm_system = static_cast<SbDrmSystem>(drm_system);
    } else {
      LOG(INFO)
          << "No SbDrmSystem was created before SbPlayer; no decryption is "
             "possible in starboard.";
      out_param.drm_system = kSbDrmSystemInvalid;
    }

    return out_param;
  }

  SbMediaVideoSampleInfo ToSbMediaVideoSampleInfo(
      const StarboardVideoSampleInfo& in_video_info) override {
    SbMediaVideoSampleInfo out_video_info = {};

    out_video_info.codec = static_cast<SbMediaVideoCodec>(in_video_info.codec);
    out_video_info.mime = in_video_info.mime;
    out_video_info.max_video_capabilities =
        in_video_info.max_video_capabilities;
    out_video_info.is_key_frame = in_video_info.is_key_frame;
    out_video_info.frame_width = in_video_info.frame_width;
    out_video_info.frame_height = in_video_info.frame_height;

    const StarboardColorMetadata& in_color_metadata =
        in_video_info.color_metadata;
    SbMediaColorMetadata& out_color_metadata = out_video_info.color_metadata;

    out_color_metadata.bits_per_channel = in_color_metadata.bits_per_channel;
    out_color_metadata.chroma_subsampling_horizontal =
        in_color_metadata.chroma_subsampling_horizontal;
    out_color_metadata.chroma_subsampling_vertical =
        in_color_metadata.chroma_subsampling_vertical;
    out_color_metadata.cb_subsampling_horizontal =
        in_color_metadata.cb_subsampling_horizontal;
    out_color_metadata.cb_subsampling_vertical =
        in_color_metadata.cb_subsampling_vertical;
    out_color_metadata.chroma_siting_horizontal =
        in_color_metadata.chroma_siting_horizontal;
    out_color_metadata.chroma_siting_vertical =
        in_color_metadata.chroma_siting_vertical;
    // note: we skip SbMediaMasteringMetadata
    out_color_metadata.max_cll = in_color_metadata.max_cll;
    out_color_metadata.max_fall = in_color_metadata.max_fall;
    out_color_metadata.primaries =
        static_cast<SbMediaPrimaryId>(in_color_metadata.primaries);
    out_color_metadata.transfer =
        static_cast<SbMediaTransferId>(in_color_metadata.transfer);
    out_color_metadata.matrix =
        static_cast<SbMediaMatrixId>(in_color_metadata.matrix);
    out_color_metadata.range =
        static_cast<SbMediaRangeId>(in_color_metadata.range);

    static_assert(sizeof(out_color_metadata.custom_primary_matrix) ==
                      sizeof(in_color_metadata.custom_primary_matrix),
                  "Struct field size mismatch (custom_primary_matrix)");
    memcpy(out_color_metadata.custom_primary_matrix,
           in_color_metadata.custom_primary_matrix,
           sizeof(out_color_metadata.custom_primary_matrix));

    return out_video_info;
  }

  SbMediaAudioSampleInfo ToSbMediaAudioSampleInfo(
      const StarboardAudioSampleInfo& in_audio_info) override {
    SbMediaAudioSampleInfo out_audio_info = {};

    out_audio_info.codec = static_cast<SbMediaAudioCodec>(in_audio_info.codec);
    out_audio_info.mime = in_audio_info.mime;
    out_audio_info.format_tag = in_audio_info.format_tag;
    out_audio_info.number_of_channels = in_audio_info.number_of_channels;
    out_audio_info.samples_per_second = in_audio_info.samples_per_second;
    out_audio_info.average_bytes_per_second =
        in_audio_info.average_bytes_per_second;
    out_audio_info.block_alignment = in_audio_info.block_alignment;
    out_audio_info.bits_per_sample = in_audio_info.bits_per_sample;
    out_audio_info.audio_specific_config_size =
        in_audio_info.audio_specific_config_size;
    out_audio_info.audio_specific_config = in_audio_info.audio_specific_config;

    return out_audio_info;
  }

  void CallWriteSamples(SbPlayer player,
                        SbMediaType sample_type,
                        const SbPlayerSampleInfo* sample_infos,
                        int number_of_sample_infos) override {
    SbPlayerWriteSample2(player, sample_type, sample_infos,
                         number_of_sample_infos);
  }
};

}  // namespace

// Note: declared in starboard_api_wrapper.h.
std::unique_ptr<StarboardApiWrapper> GetStarboardApiWrapper() {
  return std::make_unique<StarboardApiWrapper14>();
}

}  // namespace media
}  // namespace chromecast
