// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_media_controls/mac/remote_command_center_delegate_cocoa.h"

#import <MediaPlayer/MediaPlayer.h>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/system_media_controls/mac/remote_command_center_delegate.h"

@interface RemoteCommandCenterDelegateCocoa ()

- (void)setCommand:(MPRemoteCommand*)command enabled:(bool)enabled;
- (void)enableCommand:(MPRemoteCommand*)command;
- (void)disableCommand:(MPRemoteCommand*)command;

@end

@implementation RemoteCommandCenterDelegateCocoa {
  raw_ptr<system_media_controls::internal::RemoteCommandCenterDelegate>
      _delegate;
}

- (instancetype)initWithDelegate:
    (system_media_controls::internal::RemoteCommandCenterDelegate*)delegate {
  if (self = [super init]) {
    _delegate = delegate;

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
    _delegate->OnPause();
  } else if (event.command == commandCenter.playCommand) {
    _delegate->OnPlay();
  } else if (event.command == commandCenter.stopCommand) {
    _delegate->OnStop();
  } else if (event.command == commandCenter.togglePlayPauseCommand) {
    _delegate->OnPlayPause();
  } else if (event.command == commandCenter.nextTrackCommand) {
    _delegate->OnNext();
  } else if (event.command == commandCenter.previousTrackCommand) {
    _delegate->OnPrevious();
  } else if (event.command == commandCenter.changePlaybackPositionCommand) {
    MPChangePlaybackPositionCommandEvent* changePlaybackPositionCommandEvent =
        (MPChangePlaybackPositionCommandEvent*)event;
    _delegate->OnSeekTo(
        base::Seconds(changePlaybackPositionCommandEvent.positionTime));
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

- (void)setCanSeekTo:(bool)can_seek_to {
  MPRemoteCommandCenter* commandCenter =
      [MPRemoteCommandCenter sharedCommandCenter];
  [self setCommand:commandCenter.changePlaybackPositionCommand
           enabled:can_seek_to];
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
