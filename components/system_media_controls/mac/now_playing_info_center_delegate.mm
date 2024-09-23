// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_media_controls/mac/now_playing_info_center_delegate.h"

#import <MediaPlayer/MediaPlayer.h>

#include "base/functional/bind.h"
#include "base/mac/mac_util.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "components/system_media_controls/mac/now_playing_info_center_delegate_cocoa.h"
#include "skia/ext/skia_utils_mac.h"

namespace system_media_controls::internal {

namespace {

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
      NOTREACHED_IN_MIGRATION();
  }
  return MPNowPlayingPlaybackStateUnknown;
}

}  // anonymous namespace

NowPlayingInfoCenterDelegate::NowPlayingInfoCenterDelegate() {
  now_playing_info_center_delegate_cocoa_ =
      [[NowPlayingInfoCenterDelegateCocoa alloc] init];
}

NowPlayingInfoCenterDelegate::~NowPlayingInfoCenterDelegate() {
  [now_playing_info_center_delegate_cocoa_ resetNowPlayingInfo];
  timer_->Stop();
}

void NowPlayingInfoCenterDelegate::SetPlaybackStatus(
    SystemMediaControls::PlaybackStatus status) {
  playback_status_ = status;
  StartTimer();
}

void NowPlayingInfoCenterDelegate::SetTitle(const std::u16string& title) {
  [now_playing_info_center_delegate_cocoa_
      setTitle:base::SysUTF16ToNSString(title)];
  [now_playing_info_center_delegate_cocoa_ updateNowPlayingInfo];
}

void NowPlayingInfoCenterDelegate::SetArtist(const std::u16string& artist) {
  [now_playing_info_center_delegate_cocoa_
      setArtist:base::SysUTF16ToNSString(artist)];
  [now_playing_info_center_delegate_cocoa_ updateNowPlayingInfo];
}

void NowPlayingInfoCenterDelegate::SetAlbum(const std::u16string& album) {
  [now_playing_info_center_delegate_cocoa_
      setAlbum:base::SysUTF16ToNSString(album)];
  [now_playing_info_center_delegate_cocoa_ updateNowPlayingInfo];
}

void NowPlayingInfoCenterDelegate::SetThumbnail(const SkBitmap& bitmap) {
  NSImage* image = skia::SkBitmapToNSImage(bitmap);
  [now_playing_info_center_delegate_cocoa_ setThumbnail:image];
  [now_playing_info_center_delegate_cocoa_ updateNowPlayingInfo];
}

void NowPlayingInfoCenterDelegate::SetPosition(
    const media_session::MediaPosition& position) {
  position_ = position;
  StartTimer();
}

void NowPlayingInfoCenterDelegate::StartTimer() {
  timer_->Start(
      FROM_HERE, base::Milliseconds(100),
      base::BindOnce(
          &NowPlayingInfoCenterDelegate::UpdatePlaybackStatusAndPosition,
          base::Unretained(this)));
}

void NowPlayingInfoCenterDelegate::UpdatePlaybackStatusAndPosition() {
  auto position = position_.value_or(media_session::MediaPosition());
  auto playback_status =
      playback_status_.value_or(SystemMediaControls::PlaybackStatus::kStopped);

  MPNowPlayingPlaybackState state =
      PlaybackStatusToMPNowPlayingPlaybackState(playback_status);
  [now_playing_info_center_delegate_cocoa_ setPlaybackState:state];

  auto time_since_epoch =
      position.last_updated_time() - base::TimeTicks::UnixEpoch();
  [now_playing_info_center_delegate_cocoa_
      setCurrentPlaybackDate:
          [NSDate dateWithTimeIntervalSince1970:time_since_epoch.InSecondsF()]];
  [now_playing_info_center_delegate_cocoa_
      setDuration:@(position.duration().InSecondsF())];

  // If we're not currently playing, then set the rate to zero.
  double rate =
      (playback_status == SystemMediaControls::PlaybackStatus::kPlaying)
          ? position.playback_rate()
          : 0;
  [now_playing_info_center_delegate_cocoa_ setPlaybackRate:@(rate)];
  [now_playing_info_center_delegate_cocoa_
      setElapsedPlaybackTime:@(position
                                   .GetPositionAtTime(
                                       position.last_updated_time())
                                   .InSecondsF())];

  [now_playing_info_center_delegate_cocoa_ updateNowPlayingInfo];
}

void NowPlayingInfoCenterDelegate::ClearMetadata() {
  [now_playing_info_center_delegate_cocoa_ clearMetadata];
  playback_status_.reset();
  position_.reset();
  timer_->Stop();
}

}  // namespace system_media_controls::internal
