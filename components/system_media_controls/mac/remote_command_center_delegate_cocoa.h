// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_REMOTE_COMMAND_CENTER_DELEGATE_COCOA_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_REMOTE_COMMAND_CENTER_DELEGATE_COCOA_H_

#import <Cocoa/Cocoa.h>
#import <MediaPlayer/MediaPlayer.h>

namespace system_media_controls::internal {
class RemoteCommandCenterDelegate;
}  // namespace system_media_controls::internal

@interface RemoteCommandCenterDelegateCocoa : NSObject

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
- (void)setCanSeekTo:(bool)can_seek_to;

@end

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_REMOTE_COMMAND_CENTER_DELEGATE_COCOA_H_
