// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_media_controls/mac/now_playing_info_center_delegate_cocoa.h"

#import <MediaPlayer/MediaPlayer.h>

#include "base/mac/scoped_nsobject.h"
#include "build/branding_buildflags.h"

@interface NowPlayingInfoCenterDelegateCocoa ()

// Initialize the |nowPlayingInfo_| dictionary with values.
- (void)initializeNowPlayingInfoValues;

// Give MPNowPlayingInfoCenter the updated nowPlayingInfo_ dictionary.
- (void)updateNowPlayingInfo;

@end

@implementation NowPlayingInfoCenterDelegateCocoa {
  base::scoped_nsobject<NSMutableDictionary> _nowPlayingInfo;
}

- (instancetype)init {
  if (self = [super init]) {
    _nowPlayingInfo.reset([[NSMutableDictionary alloc] init]);
    [self resetNowPlayingInfo];
    [self updateNowPlayingInfo];
  }
  return self;
}

- (void)resetNowPlayingInfo {
  [_nowPlayingInfo removeAllObjects];
  [self initializeNowPlayingInfoValues];
  [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = nil;
}

- (void)setPlaybackState:(MPNowPlayingPlaybackState)state {
  [MPNowPlayingInfoCenter defaultCenter].playbackState = state;
  [self updateNowPlayingInfo];
}

- (void)setTitle:(NSString*)title {
  [_nowPlayingInfo setObject:title forKey:MPMediaItemPropertyTitle];
  [self updateNowPlayingInfo];
}

- (void)setArtist:(NSString*)artist {
  [_nowPlayingInfo setObject:artist forKey:MPMediaItemPropertyArtist];
  [self updateNowPlayingInfo];
}

- (void)setAlbum:(NSString*)album {
  [_nowPlayingInfo setObject:album forKey:MPMediaItemPropertyAlbumTitle];
  [self updateNowPlayingInfo];
}

- (void)setPlaybackRate:(NSNumber*)rate {
  [_nowPlayingInfo setObject:rate forKey:MPNowPlayingInfoPropertyPlaybackRate];
  [self updateNowPlayingInfo];
}

- (void)setCurrentPlaybackDate:(NSDate*)date {
  [_nowPlayingInfo setObject:date
                      forKey:MPNowPlayingInfoPropertyCurrentPlaybackDate];
  [self updateNowPlayingInfo];
}

- (void)setElapsedPlaybackTime:(NSNumber*)time {
  [_nowPlayingInfo setObject:time
                      forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
  [self updateNowPlayingInfo];
}

- (void)setDuration:(NSNumber*)duration {
  [_nowPlayingInfo setObject:duration
                      forKey:MPMediaItemPropertyPlaybackDuration];
  [self updateNowPlayingInfo];
}

- (void)clearMetadata {
  [self initializeNowPlayingInfoValues];
  [self updateNowPlayingInfo];
}

- (void)initializeNowPlayingInfoValues {
  [_nowPlayingInfo setObject:[NSNumber numberWithDouble:0]
                      forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
  [_nowPlayingInfo setObject:[NSNumber numberWithDouble:0]
                      forKey:MPNowPlayingInfoPropertyPlaybackRate];
  [_nowPlayingInfo setObject:[NSNumber numberWithDouble:0]
                      forKey:MPMediaItemPropertyPlaybackDuration];
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  [_nowPlayingInfo setObject:@"Chrome" forKey:MPMediaItemPropertyTitle];
#else
  [_nowPlayingInfo setObject:@"Chromium" forKey:MPMediaItemPropertyTitle];
#endif
  [_nowPlayingInfo setObject:@"" forKey:MPMediaItemPropertyArtist];
  [_nowPlayingInfo setObject:@"" forKey:MPMediaItemPropertyAlbumTitle];
}

- (void)updateNowPlayingInfo {
  [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = _nowPlayingInfo;
}

@end
