// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_SYSTEM_MEDIA_CONTROLS_MAC_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_SYSTEM_MEDIA_CONTROLS_MAC_H_

#include <memory>

#include "components/system_media_controls/mac/now_playing_info_center_delegate.h"
#include "components/system_media_controls/mac/remote_command_center_delegate.h"
#include "components/system_media_controls/system_media_controls.h"

namespace system_media_controls {

class SystemMediaControlsObserver;

namespace internal {

// Interfaces with Mac OS's MPNowPlayingInfoCenter and related MediaPlayer API.
// The combination of those two form the full SystemMediaControls API.
class API_AVAILABLE(macos(10.13.1)) SystemMediaControlsMac
    : public SystemMediaControls {
 public:
  SystemMediaControlsMac();
  SystemMediaControlsMac(const SystemMediaControlsMac&) = delete;
  SystemMediaControlsMac& operator=(const SystemMediaControlsMac&) = delete;
  ~SystemMediaControlsMac() override;

  // SystemMediaControls implementation.
  void AddObserver(SystemMediaControlsObserver* observer) override;
  void RemoveObserver(SystemMediaControlsObserver* observer) override;
  void SetEnabled(bool enabled) override {}
  void SetIsNextEnabled(bool value) override;
  void SetIsPreviousEnabled(bool value) override;
  void SetIsPlayPauseEnabled(bool value) override;
  void SetIsStopEnabled(bool value) override;
  void SetIsSeekToEnabled(bool value) override;
  void SetPlaybackStatus(PlaybackStatus status) override;
  void SetTitle(const std::u16string& title) override;
  void SetArtist(const std::u16string& artist) override;
  void SetAlbum(const std::u16string& album) override;
  void SetThumbnail(const SkBitmap& bitmap) override;
  void SetPosition(const media_session::MediaPosition& position) override;
  void ClearThumbnail() override {}
  void ClearMetadata() override;
  void UpdateDisplay() override {}

 private:
  // Gives media playback state and metadata to the MPNowPlayingInfoCenter.
  NowPlayingInfoCenterDelegate now_playing_info_center_delegate_;

  // Receives media events (e.g. play/pause controls from the user) and sends
  // them to observers. Also keeps the system informed of which media controls
  // are currently supported.
  RemoteCommandCenterDelegate remote_command_center_delegate_;
};

}  // namespace internal
}  // namespace system_media_controls

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_SYSTEM_MEDIA_CONTROLS_MAC_H_
