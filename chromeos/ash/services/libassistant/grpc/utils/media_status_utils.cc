// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/utils/media_status_utils.h"

#include "chromeos/ash/services/libassistant/public/mojom/media_controller.mojom.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"

namespace ash::libassistant {

namespace {

using PlaybackState = assistant_client::MediaStatus::PlaybackState;
using TrackType = assistant_client::TrackType;

PlaybackState ConvertPlaybackStateEnumToV1FromV2(
    const MediaStatus::PlaybackState& state_proto) {
  switch (state_proto) {
    // New value in V2, treat as IDLE.
    case MediaStatus::UNSPECIFIED:
      return PlaybackState::IDLE;
    case MediaStatus::IDLE:
      return PlaybackState::IDLE;
    case MediaStatus::NEW_TRACK:
      return PlaybackState::NEW_TRACK;
    case MediaStatus::PLAYING:
      return PlaybackState::PLAYING;
    case MediaStatus::PAUSED:
      return PlaybackState::PAUSED;
    case MediaStatus::ERROR:
      return PlaybackState::ERROR;
  }
}

TrackType ConvertTrackTypeEnumToV1FromV2(
    const MediaStatus::TrackType& track_type_proto) {
  switch (track_type_proto) {
    // New value in V2, treat as NONE.
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

MediaStatus::PlaybackState ConvertPlaybackStateEnumToV2FromV1(
    const PlaybackState& state) {
  switch (state) {
    case PlaybackState::IDLE:
      return MediaStatus::IDLE;
    case PlaybackState::NEW_TRACK:
      return MediaStatus::NEW_TRACK;
    case PlaybackState::PLAYING:
      return MediaStatus::PLAYING;
    case PlaybackState::PAUSED:
      return MediaStatus::PAUSED;
    case PlaybackState::ERROR:
      return MediaStatus::ERROR;
  }
}

MediaStatus::TrackType ConvertTrackTypeEnumToV2FromV1(
    const TrackType& track_type) {
  switch (track_type) {
    case TrackType::MEDIA_TRACK_NONE:
      return MediaStatus::MEDIA_TRACK_NONE;
    case TrackType::MEDIA_TRACK_TTS:
      return MediaStatus::MEDIA_TRACK_TTS;
    case TrackType::MEDIA_TRACK_CONTENT:
      return MediaStatus::MEDIA_TRACK_CONTENT;
  }
}

MediaStatus::PlaybackState ConvertPlaybackStateEnumToV2FromMojom(
    const mojom::PlaybackState& state) {
  switch (state) {
    case mojom::PlaybackState::kIdle:
      return MediaStatus::IDLE;
    case mojom::PlaybackState::kNewTrack:
      return MediaStatus::NEW_TRACK;
    case mojom::PlaybackState::kPlaying:
      return MediaStatus::PLAYING;
    case mojom::PlaybackState::kPaused:
      return MediaStatus::PAUSED;
    case mojom::PlaybackState::kError:
      return MediaStatus::ERROR;
  }
}

mojom::PlaybackState ConvertPlaybackStateEnumToMojomFromV2(
    const MediaStatus::PlaybackState& state) {
  switch (state) {
    // New value in V2, treat as kIdle.
    case MediaStatus::UNSPECIFIED:
      return mojom::PlaybackState::kIdle;
    case MediaStatus::IDLE:
      return mojom::PlaybackState::kIdle;
    case MediaStatus::NEW_TRACK:
      return mojom::PlaybackState::kNewTrack;
    case MediaStatus::PLAYING:
      return mojom::PlaybackState::kPlaying;
    case MediaStatus::PAUSED:
      return mojom::PlaybackState::kPaused;
    case MediaStatus::ERROR:
      return mojom::PlaybackState::kError;
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

void ConvertMediaStatusToV2FromV1(
    const assistant_client::MediaStatus& media_status,
    MediaStatus* media_status_proto) {
  media_status_proto->set_playback_state(
      ConvertPlaybackStateEnumToV2FromV1(media_status.playback_state));

  auto* media_metadata_proto = media_status_proto->mutable_metadata();
  auto media_metadata = media_status.metadata;
  media_metadata_proto->set_album(media_metadata.album);
  media_metadata_proto->set_album_art_url(media_metadata.album_art_url);
  media_metadata_proto->set_artist(media_metadata.artist);
  media_metadata_proto->set_title(media_metadata.title);
  media_metadata_proto->set_duration_ms(media_metadata.duration_ms);

  media_status_proto->set_track_type(
      ConvertTrackTypeEnumToV2FromV1(media_status.track_type));
  media_status_proto->set_position_ms(media_status.position_ms);
}

mojom::MediaStatePtr ConvertMediaStatusToMojomFromV2(
    const MediaStatus& media_status_proto) {
  mojom::MediaStatePtr state_ptr = mojom::MediaState::New();

  if (!media_status_proto.metadata().album().empty() ||
      !media_status_proto.metadata().title().empty() ||
      !media_status_proto.metadata().artist().empty()) {
    state_ptr->metadata = mojom::MediaMetadata::New();
    state_ptr->metadata->album = media_status_proto.metadata().album();
    state_ptr->metadata->title = media_status_proto.metadata().title();
    state_ptr->metadata->artist = media_status_proto.metadata().artist();
  }
  state_ptr->playback_state = ConvertPlaybackStateEnumToMojomFromV2(
      media_status_proto.playback_state());
  return state_ptr;
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

}  // namespace ash::libassistant
