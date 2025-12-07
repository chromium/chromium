// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_media_controls/mac/remote_cocoa/system_media_controls_bridge.h"

#import <Cocoa/Cocoa.h>
#import <MediaPlayer/MediaPlayer.h>

#include "components/system_media_controls/system_media_controls.h"
#include "services/media_session/public/cpp/media_position.h"

namespace {
system_media_controls::SystemMediaControls::PlaybackStatus
ConvertPlaybackStatus(system_media_controls::mojom::PlaybackStatus status) {
  switch (status) {
    case system_media_controls::mojom::PlaybackStatus::kPlaying:
      return system_media_controls::SystemMediaControls::PlaybackStatus::
          kPlaying;
    case system_media_controls::mojom::PlaybackStatus::kPaused:
      return system_media_controls::SystemMediaControls::PlaybackStatus::
          kPaused;
    case system_media_controls::mojom::PlaybackStatus::kStopped:
      return system_media_controls::SystemMediaControls::PlaybackStatus::
          kStopped;
  }
}
}  // namespace

namespace system_media_controls {
SystemMediaControlsBridge::SystemMediaControlsBridge(
    mojo::PendingReceiver<mojom::SystemMediaControls> receiver,
    mojo::PendingRemote<mojom::SystemMediaControlsObserver> remote) {
  BindMojoConnections(std::move(receiver), std::move(remote));

  // Notify test observers that the bridge was created.
  remote_command_center_delegate_.OnBridgeCreatedForTesting();
}

void SystemMediaControlsBridge::BindMojoConnections(
    mojo::PendingReceiver<mojom::SystemMediaControls> receiver,
    mojo::PendingRemote<mojom::SystemMediaControlsObserver> remote) {
  remote_command_center_delegate_.BindObserverRemote(std::move(remote));
  if (receiver_.is_bound()) {
    receiver_.reset();
  }
  receiver_.Bind(std::move(receiver));
  DCHECK(receiver_.is_bound());
}

SystemMediaControlsBridge::~SystemMediaControlsBridge() = default;

void SystemMediaControlsBridge::SetIsNextEnabled(bool enabled) {
  remote_command_center_delegate_.SetIsNextEnabled(enabled);
}

void SystemMediaControlsBridge::SetIsPreviousEnabled(bool enabled) {
  remote_command_center_delegate_.SetIsPreviousEnabled(enabled);
}

void SystemMediaControlsBridge::SetIsPlayPauseEnabled(bool enabled) {
  remote_command_center_delegate_.SetIsPlayPauseEnabled(enabled);
}

void SystemMediaControlsBridge::SetIsStopEnabled(bool enabled) {
  remote_command_center_delegate_.SetIsStopEnabled(enabled);
}

void SystemMediaControlsBridge::SetIsSeekToEnabled(bool enabled) {
  remote_command_center_delegate_.SetIsSeekToEnabled(enabled);
}

void SystemMediaControlsBridge::SetPlaybackStatus(
    mojom::PlaybackStatus status) {
  now_playing_info_center_delegate_.SetPlaybackStatus(
      ConvertPlaybackStatus(status));
}

void SystemMediaControlsBridge::SetTitle(const std::u16string& title) {
  now_playing_info_center_delegate_.SetTitle(title);
}

void SystemMediaControlsBridge::SetArtist(const std::u16string& artist) {
  now_playing_info_center_delegate_.SetArtist(artist);
}

void SystemMediaControlsBridge::SetAlbum(const std::u16string& album) {
  now_playing_info_center_delegate_.SetAlbum(album);
}

void SystemMediaControlsBridge::SetThumbnail(const SkBitmap& thumbnail) {
  now_playing_info_center_delegate_.SetThumbnail(thumbnail);
}

void SystemMediaControlsBridge::SetPosition(
    const media_session::MediaPosition& position) {
  now_playing_info_center_delegate_.SetPosition(position);
}

void SystemMediaControlsBridge::ClearMetadata() {
  now_playing_info_center_delegate_.ClearMetadata();

  // Notify test observers that metadata has been cleared.
  remote_command_center_delegate_.OnMetadataClearedForTesting();
}

}  // namespace system_media_controls
