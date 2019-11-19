// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_media_controls/mac/remote_command_center_delegate_cocoa.h"

#import <MediaPlayer/MediaPlayer.h>

#include "components/system_media_controls/mac/remote_command_center_delegate.h"

API_AVAILABLE(macos(10.12.2))
@interface RemoteCommandCenterDelegateCocoa ()

- (void)setCommand:(MPRemoteCommand*)command enabled:(bool)enabled;
- (void)enableCommand:(MPRemoteCommand*)command;
- (void)disableCommand:(MPRemoteCommand*)command;

@end

@implementation RemoteCommandCenterDelegateCocoa

- (instancetype)initWithDelegate:
    (system_media_controls::internal::RemoteCommandCenterDelegate*)delegate {
  if (self = [super init]) {
    delegate_ = delegate;

    // Initialize all commands as disabled.
    MPRemoteCommandCenter* commandCenter =
        [MPRemoteCommandCenter sharedCommandCenter];
    commandCenter.pauseCommand.enabled = NO;
    commandCenter.playCommand.enabled = NO;
    commandCenter.stopCommand.enabled = NO;
    commandCenter.togglePlayPauseCommand.enabled = NO;
    commandCenter.nextTrackCommand.enabled = NO;
    commandCenter.previousTrackCommand.enabled = NO;
    commandCenter.changeRepeatModeCommand.enabled = NO;
    commandCenter.changeShuffleModeCommand.enabled = NO;
    commandCenter.changePlaybackRateCommand.enabled = NO;
    commandCenter.seekBackwardCommand.enabled = NO;
    commandCenter.seekForwardCommand.enabled = NO;
    commandCenter.skipBackwardCommand.enabled = NO;
    commandCenter.skipForwardCommand.enabled = NO;
    commandCenter.changePlaybackPositionCommand.enabled = NO;
    commandCenter.ratingCommand.enabled = NO;
    commandCenter.likeCommand.enabled = NO;
    commandCenter.dislikeCommand.enabled = NO;
    commandCenter.bookmarkCommand.enabled = NO;
    commandCenter.enableLanguageOptionCommand.enabled = NO;
    commandCenter.disableLanguageOptionCommand.enabled = NO;
  }
  return self;
}

- (MPRemoteCommandHandlerStatus)onCommand:(MPRemoteCommandEvent*)event {
  MPRemoteCommandCenter* commandCenter =
      [MPRemoteCommandCenter sharedCommandCenter];
  if (event.command == commandCenter.pauseCommand) {
    delegate_->OnPause();
  } else if (event.command == commandCenter.playCommand) {
    delegate_->OnPlay();
  } else if (event.command == commandCenter.stopCommand) {
    delegate_->OnStop();
  } else if (event.command == commandCenter.togglePlayPauseCommand) {
    delegate_->OnPlayPause();
  } else if (event.command == commandCenter.nextTrackCommand) {
    delegate_->OnNext();
  } else if (event.command == commandCenter.previousTrackCommand) {
    delegate_->OnPrevious();
  }
  return MPRemoteCommandHandlerStatusSuccess;
}

- (void)setCanPlay:(bool)can_play {
  MPRemoteCommandCenter* commandCenter =
      [MPRemoteCommandCenter sharedCommandCenter];
  [self setCommand:commandCenter.playCommand enabled:can_play];
}

- (void)setCanPause:(bool)can_pause {
  MPRemoteCommandCenter* commandCenter =
      [MPRemoteCommandCenter sharedCommandCenter];
  [self setCommand:commandCenter.pauseCommand enabled:can_pause];
}

- (void)setCanStop:(bool)can_stop {
  MPRemoteCommandCenter* commandCenter =
      [MPRemoteCommandCenter sharedCommandCenter];
  [self setCommand:commandCenter.stopCommand enabled:can_stop];
}

- (void)setCanPlayPause:(bool)can_playpause {
  MPRemoteCommandCenter* commandCenter =
      [MPRemoteCommandCenter sharedCommandCenter];
  [self setCommand:commandCenter.togglePlayPauseCommand enabled:can_playpause];
}

- (void)setCanGoNextTrack:(bool)can_go_next_track {
  MPRemoteCommandCenter* commandCenter =
      [MPRemoteCommandCenter sharedCommandCenter];
  [self setCommand:commandCenter.nextTrackCommand enabled:can_go_next_track];
}

- (void)setCanGoPreviousTrack:(bool)can_go_prev_track {
  MPRemoteCommandCenter* commandCenter =
      [MPRemoteCommandCenter sharedCommandCenter];
  [self setCommand:commandCenter.previousTrackCommand
           enabled:can_go_prev_track];
}

- (void)setCommand:(MPRemoteCommand*)command enabled:(bool)enabled {
  if (enabled) {
    [self enableCommand:command];
  } else {
    [self disableCommand:command];
  }
}

- (void)enableCommand:(MPRemoteCommand*)command {
  command.enabled = YES;
  [command addTarget:self action:@selector(onCommand:)];
}

- (void)disableCommand:(MPRemoteCommand*)command {
  command.enabled = NO;
  [command removeTarget:self];
}

@end
