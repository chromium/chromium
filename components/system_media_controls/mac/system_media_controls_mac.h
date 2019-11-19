// Copyright 2019 The Chromium Authors. All rights reserved.
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
class API_AVAILABLE(macos(10.12.2)) SystemMediaControlsMac
    : public SystemMediaControls {
 public:
  SystemMediaControlsMac();
  SystemMediaControlsMac(const SystemMediaControlsMac&) = delete;
  SystemMediaControlsMac& operator=(const SystemMediaControlsMac&) = delete;
  ~SystemMediaControlsMac() override;

  static SystemMediaControlsMac* GetInstance();

  // SystemMediaControls implementation.
  void AddObserver(SystemMediaControlsObserver* observer) override;
  void RemoveObserver(SystemMediaControlsObserver* observer) override;
  void SetEnabled(bool enabled) override {}
  void SetIsNextEnabled(bool value) override;
  void SetIsPreviousEnabled(bool value) override;
  void SetIsPlayPauseEnabled(bool value) override;
  void SetIsStopEnabled(bool value) override;
  void SetPlaybackStatus(PlaybackStatus status) override;
  void SetTitle(const base::string16& title) override;
  void SetArtist(const base::string16& artist) override;
  void SetAlbum(const base::string16& album) override;
  void SetThumbnail(const SkBitmap& bitmap) override {}
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
