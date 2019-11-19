// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_NOW_PLAYING_INFO_CENTER_DELEGATE_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_NOW_PLAYING_INFO_CENTER_DELEGATE_H_

#include "base/mac/scoped_nsobject.h"
#include "components/system_media_controls/system_media_controls.h"

@class NowPlayingInfoCenterDelegateCocoa;

namespace system_media_controls {
namespace internal {

// Wraps an NSObject which interfaces with the MPNowPlayingInfoCenter.
class API_AVAILABLE(macos(10.12.2)) NowPlayingInfoCenterDelegate {
 public:
  NowPlayingInfoCenterDelegate();
  NowPlayingInfoCenterDelegate(const NowPlayingInfoCenterDelegate&) = delete;
  NowPlayingInfoCenterDelegate& operator=(const NowPlayingInfoCenterDelegate&) =
      delete;
  ~NowPlayingInfoCenterDelegate();

  // Part of the implementation of SystemMediaControls.
  void SetPlaybackStatus(SystemMediaControls::PlaybackStatus status);
  void SetTitle(const base::string16& title);
  void SetArtist(const base::string16& artist);
  void SetAlbum(const base::string16& album);
  void ClearMetadata();

 private:
  base::scoped_nsobject<NowPlayingInfoCenterDelegateCocoa>
      now_playing_info_center_delegate_cocoa_;
};

}  // namespace internal
}  // namespace system_media_controls

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_NOW_PLAYING_INFO_CENTER_DELEGATE_H_
