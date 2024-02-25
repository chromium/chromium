// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_NOW_PLAYING_INFO_CENTER_DELEGATE_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_NOW_PLAYING_INFO_CENTER_DELEGATE_H_

#include <optional>

#include "base/timer/timer.h"
#include "components/system_media_controls/system_media_controls.h"

@class NowPlayingInfoCenterDelegateCocoa;

namespace system_media_controls::internal {

// Wraps an NSObject which interfaces with the MPNowPlayingInfoCenter.
class NowPlayingInfoCenterDelegate {
 public:
  NowPlayingInfoCenterDelegate();
  NowPlayingInfoCenterDelegate(const NowPlayingInfoCenterDelegate&) = delete;
  NowPlayingInfoCenterDelegate& operator=(const NowPlayingInfoCenterDelegate&) =
      delete;
  ~NowPlayingInfoCenterDelegate();

  // Part of the implementation of SystemMediaControls.
  void SetPlaybackStatus(SystemMediaControls::PlaybackStatus status);
  void SetTitle(const std::u16string& title);
  void SetArtist(const std::u16string& artist);
  void SetAlbum(const std::u16string& album);
  void SetThumbnail(const SkBitmap& bitmap);
  void SetPosition(const media_session::MediaPosition& position);
  void ClearMetadata();

 private:
  // Starts timer to update the current playback status and position. This
  // debounce timer is mandatory as the now playing widget doesn't work properly
  // when updated multiple times in a row.
  void StartTimer();

  // Updates the current playback status and position depending on most recently
  // received playback status and position.
  void UpdatePlaybackStatusAndPosition();

  // Stores the most recently received playback status.
  std::optional<SystemMediaControls::PlaybackStatus> playback_status_;

  // Stores the most recently received position.
  std::optional<media_session::MediaPosition> position_;

  // Calls UpdatePlaybackStatusAndPosition() when the timer expires.
  std::unique_ptr<base::OneShotTimer> timer_ =
      std::make_unique<base::OneShotTimer>();

  NowPlayingInfoCenterDelegateCocoa* __strong
      now_playing_info_center_delegate_cocoa_;
};

}  // namespace system_media_controls::internal

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_NOW_PLAYING_INFO_CENTER_DELEGATE_H_
