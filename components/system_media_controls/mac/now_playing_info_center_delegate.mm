// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_media_controls/mac/now_playing_info_center_delegate.h"

#import <MediaPlayer/MediaPlayer.h>

#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "components/system_media_controls/mac/now_playing_info_center_delegate_cocoa.h"

namespace system_media_controls {
namespace internal {

namespace {

API_AVAILABLE(macos(10.13.1))
MPNowPlayingPlaybackState PlaybackStatusToMPNowPlayingPlaybackState(
    SystemMediaControls::PlaybackStatus status) {
  switch (status) {
    case SystemMediaControls::PlaybackStatus::kPlaying:
      return MPNowPlayingPlaybackStatePlaying;
    case SystemMediaControls::PlaybackStatus::kPaused:
      return MPNowPlayingPlaybackStatePaused;
    case SystemMediaControls::PlaybackStatus::kStopped:
      return MPNowPlayingPlaybackStateStopped;
    default:
      NOTREACHED();
  }
  return MPNowPlayingPlaybackStateUnknown;
}

}  // anonymous namespace

NowPlayingInfoCenterDelegate::NowPlayingInfoCenterDelegate() {
  now_playing_info_center_delegate_cocoa_.reset(
      [[NowPlayingInfoCenterDelegateCocoa alloc] init]);
}

NowPlayingInfoCenterDelegate::~NowPlayingInfoCenterDelegate() {
  [now_playing_info_center_delegate_cocoa_ resetNowPlayingInfo];
}

void NowPlayingInfoCenterDelegate::SetPlaybackStatus(
    SystemMediaControls::PlaybackStatus status) {
  MPNowPlayingPlaybackState state =
      PlaybackStatusToMPNowPlayingPlaybackState(status);
  [now_playing_info_center_delegate_cocoa_ setPlaybackState:state];
}

void NowPlayingInfoCenterDelegate::SetTitle(const base::string16& title) {
  [now_playing_info_center_delegate_cocoa_
      setTitle:base::SysUTF16ToNSString(title)];
}

void NowPlayingInfoCenterDelegate::SetArtist(const base::string16& artist) {
  [now_playing_info_center_delegate_cocoa_
      setArtist:base::SysUTF16ToNSString(artist)];
}

void NowPlayingInfoCenterDelegate::SetAlbum(const base::string16& album) {
  [now_playing_info_center_delegate_cocoa_
      setAlbum:base::SysUTF16ToNSString(album)];
}

void NowPlayingInfoCenterDelegate::SetPosition(
    const media_session::MediaPosition& position) {
  auto time_since_epoch =
      position.last_updated_time() - base::TimeTicks::UnixEpoch();

  [now_playing_info_center_delegate_cocoa_
      setPlaybackRate:[NSNumber numberWithDouble:position.playback_rate()]];
  [now_playing_info_center_delegate_cocoa_
      setCurrentPlaybackDate:
          [NSDate dateWithTimeIntervalSince1970:time_since_epoch.InSecondsF()]];
  [now_playing_info_center_delegate_cocoa_
      setElapsedPlaybackTime:
          [NSNumber numberWithFloat:position
                                        .GetPositionAtTime(
                                            position.last_updated_time())
                                        .InSecondsF()]];
  [now_playing_info_center_delegate_cocoa_
      setDuration:[NSNumber numberWithFloat:position.duration().InSecondsF()]];
}

void NowPlayingInfoCenterDelegate::ClearMetadata() {
  [now_playing_info_center_delegate_cocoa_ clearMetadata];
}

}  // namespace internal
}  // namespace system_media_controls
