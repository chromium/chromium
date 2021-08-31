// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/grpc/utils/media_status_utils.h"

#include "chromeos/services/libassistant/public/mojom/media_controller.mojom.h"
#include "libassistant/shared/public/media_manager.h"

namespace chromeos {
namespace libassistant {

namespace {

using PlaybackState = assistant_client::MediaStatus::PlaybackState;
using TrackType = assistant_client::TrackType;

PlaybackState ConvertPlaybackStateEnumToV1FromV2(
    const MediaStatus::PlaybackState& state_proto) {
  switch (state_proto) {
    case MediaStatus::UNSPECIFIED:
      return assistant_client::MediaStatus::PlaybackState::IDLE;
    case MediaStatus::IDLE:
      return assistant_client::MediaStatus::PlaybackState::IDLE;
    case MediaStatus::NEW_TRACK:
      return assistant_client::MediaStatus::PlaybackState::NEW_TRACK;
    case MediaStatus::PLAYING:
      return assistant_client::MediaStatus::PlaybackState::PLAYING;
    case MediaStatus::PAUSED:
      return assistant_client::MediaStatus::PlaybackState::PAUSED;
    case MediaStatus::ERROR:
      return assistant_client::MediaStatus::PlaybackState::ERROR;
  }
}

TrackType ConvertTrackTypeEnumToV1FromV2(
    const MediaStatus::TrackType& track_type_proto) {
  switch (track_type_proto) {
    case MediaStatus::UNSUPPORTED:
      return TrackType::MEDIA_TRACK_NONE;
    case MediaStatus::MEDIA_TRACK_NONE:
      return TrackType::MEDIA_TRACK_NONE;
    case MediaStatus::MEDIA_TRACK_TTS:
      return TrackType::MEDIA_TRACK_TTS;
    case MediaStatus::MEDIA_TRACK_CONTENT:
      return TrackType::MEDIA_TRACK_CONTENT;
  }
}

MediaStatus::PlaybackState ConvertPlaybackStateEnumToV2FromMojom(
    const mojom::PlaybackState& state) {
  switch (state) {
    case mojom::PlaybackState::kError:
      return MediaStatus::ERROR;
    case mojom::PlaybackState::kIdle:
      return MediaStatus::IDLE;
    case mojom::PlaybackState::kNewTrack:
      return MediaStatus::NEW_TRACK;
    case mojom::PlaybackState::kPaused:
      return MediaStatus::PAUSED;
    case mojom::PlaybackState::kPlaying:
      return MediaStatus::PLAYING;
  }
}

}  // namespace

void ConvertMediaStatusToV1FromV2(const MediaStatus& media_status_proto,
                                  assistant_client::MediaStatus* media_status) {
  media_status->playback_state =
      ConvertPlaybackStateEnumToV1FromV2(media_status_proto.playback_state());

  assistant_client::MediaMetadata media_metadata;
  const auto& metadata_proto = media_status_proto.metadata();
  media_metadata.album = metadata_proto.album();
  media_metadata.album_art_url = metadata_proto.album_art_url();
  media_metadata.artist = metadata_proto.artist();
  media_metadata.title = metadata_proto.title();
  media_metadata.duration_ms = metadata_proto.duration_ms();
  media_status->metadata = std::move(media_metadata);

  media_status->track_type =
      ConvertTrackTypeEnumToV1FromV2(media_status_proto.track_type());
  media_status->position_ms = media_status_proto.position_ms();
}

void ConvertMediaStatusToV2FromMojom(const mojom::MediaState& state,
                                     MediaStatus* media_status_proto) {
  media_status_proto->set_playback_state(
      ConvertPlaybackStateEnumToV2FromMojom(state.playback_state));

  if (state.metadata) {
    auto* media_metadata_proto = media_status_proto->mutable_metadata();
    media_metadata_proto->set_album(state.metadata->album);
    media_metadata_proto->set_artist(state.metadata->artist);
    media_metadata_proto->set_title(state.metadata->title);
  }
}

}  // namespace libassistant
}  // namespace chromeos
