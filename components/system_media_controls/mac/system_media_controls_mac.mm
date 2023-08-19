// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_media_controls/mac/system_media_controls_mac.h"

namespace system_media_controls {

// static
std::unique_ptr<SystemMediaControls> SystemMediaControls::Create(
    const std::string& product_name) {
  return std::make_unique<internal::SystemMediaControlsMac>();
}

namespace internal {

SystemMediaControlsMac::SystemMediaControlsMac() = default;

SystemMediaControlsMac::~SystemMediaControlsMac() = default;

void SystemMediaControlsMac::AddObserver(
    SystemMediaControlsObserver* observer) {
  remote_command_center_delegate_.AddObserver(observer);
}

void SystemMediaControlsMac::RemoveObserver(
    SystemMediaControlsObserver* observer) {
  remote_command_center_delegate_.RemoveObserver(observer);
}

void SystemMediaControlsMac::SetIsNextEnabled(bool value) {
  remote_command_center_delegate_.SetIsNextEnabled(value);
}

void SystemMediaControlsMac::SetIsPreviousEnabled(bool value) {
  remote_command_center_delegate_.SetIsPreviousEnabled(value);
}

void SystemMediaControlsMac::SetIsPlayPauseEnabled(bool value) {
  remote_command_center_delegate_.SetIsPlayPauseEnabled(value);
}

void SystemMediaControlsMac::SetIsStopEnabled(bool value) {
  remote_command_center_delegate_.SetIsStopEnabled(value);
}

void SystemMediaControlsMac::SetIsSeekToEnabled(bool value) {
  remote_command_center_delegate_.SetIsSeekToEnabled(value);
}

void SystemMediaControlsMac::SetPlaybackStatus(PlaybackStatus status) {
  now_playing_info_center_delegate_.SetPlaybackStatus(status);
}

void SystemMediaControlsMac::SetTitle(const std::u16string& title) {
  now_playing_info_center_delegate_.SetTitle(title);
}

void SystemMediaControlsMac::SetArtist(const std::u16string& artist) {
  now_playing_info_center_delegate_.SetArtist(artist);
}

void SystemMediaControlsMac::SetAlbum(const std::u16string& album) {
  now_playing_info_center_delegate_.SetAlbum(album);
}

void SystemMediaControlsMac::SetThumbnail(const SkBitmap& bitmap) {
  now_playing_info_center_delegate_.SetThumbnail(bitmap);
}

void SystemMediaControlsMac::SetPosition(
    const media_session::MediaPosition& position) {
  now_playing_info_center_delegate_.SetPosition(position);
}

void SystemMediaControlsMac::ClearMetadata() {
  now_playing_info_center_delegate_.ClearMetadata();
}

}  // namespace internal
}  // namespace system_media_controls
