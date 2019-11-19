// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_REMOTE_COMMAND_CENTER_DELEGATE_COCOA_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_REMOTE_COMMAND_CENTER_DELEGATE_COCOA_H_

#import <Cocoa/Cocoa.h>
#import <MediaPlayer/MediaPlayer.h>

namespace system_media_controls {
namespace internal {
class RemoteCommandCenterDelegate;
}  // namespace internal
}  // namespace system_media_controls

API_AVAILABLE(macos(10.12.2))
@interface RemoteCommandCenterDelegateCocoa : NSObject {
 @private
  system_media_controls::internal::RemoteCommandCenterDelegate* delegate_;
}

- (instancetype)initWithDelegate:
    (system_media_controls::internal::RemoteCommandCenterDelegate*)delegate;

// Called by the OS via the MPRemoteCommandCenter.
- (MPRemoteCommandHandlerStatus)onCommand:(MPRemoteCommandEvent*)event;

// Called by the RemoteCommandCenterDelegate to enable/disable different
// commands.
- (void)setCanPlay:(bool)can_play;
- (void)setCanPause:(bool)can_pause;
- (void)setCanStop:(bool)can_stop;
- (void)setCanPlayPause:(bool)can_playpause;
- (void)setCanGoNextTrack:(bool)can_go_next_track;
- (void)setCanGoPreviousTrack:(bool)can_go_prev_track;

@end

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_REMOTE_COMMAND_CENTER_DELEGATE_COCOA_H_
