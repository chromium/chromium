// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_media_controls/mac/system_media_controls_mac.h"

#include "base/memory/singleton.h"

namespace system_media_controls {

// static
SystemMediaControls* SystemMediaControls::GetInstance() {
  // The required APIs for interacting with the Now Playing Info Center only
  // exist on 10.12.2 or later.
  if (@available(macOS 10.12.2, *))
    return internal::SystemMediaControlsMac::GetInstance();
  return nullptr;
}

namespace internal {

// static
SystemMediaControlsMac* SystemMediaControlsMac::GetInstance() {
  return base::Singleton<SystemMediaControlsMac>::get();
}

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

void SystemMediaControlsMac::SetPlaybackStatus(PlaybackStatus status) {
  now_playing_info_center_delegate_.SetPlaybackStatus(status);
}

void SystemMediaControlsMac::SetTitle(const base::string16& title) {
  now_playing_info_center_delegate_.SetTitle(title);
}

void SystemMediaControlsMac::SetArtist(const base::string16& artist) {
  now_playing_info_center_delegate_.SetArtist(artist);
}

void SystemMediaControlsMac::SetAlbum(const base::string16& album) {
  now_playing_info_center_delegate_.SetAlbum(album);
}

void SystemMediaControlsMac::ClearMetadata() {
  now_playing_info_center_delegate_.ClearMetadata();
}

}  // namespace internal
}  // namespace system_media_controls
