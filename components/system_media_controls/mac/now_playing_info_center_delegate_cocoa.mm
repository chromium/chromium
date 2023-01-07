// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_media_controls/mac/now_playing_info_center_delegate_cocoa.h"

#import <MediaPlayer/MediaPlayer.h>

#include "base/mac/scoped_nsobject.h"

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
}

- (void)setTitle:(NSString*)title {
  [_nowPlayingInfo setObject:title forKey:MPMediaItemPropertyTitle];
}

- (void)setArtist:(NSString*)artist {
  [_nowPlayingInfo setObject:artist forKey:MPMediaItemPropertyArtist];
}

- (void)setAlbum:(NSString*)album {
  [_nowPlayingInfo setObject:album forKey:MPMediaItemPropertyAlbumTitle];
}

- (void)setPlaybackRate:(NSNumber*)rate {
  [_nowPlayingInfo setObject:rate forKey:MPNowPlayingInfoPropertyPlaybackRate];
}

- (void)setCurrentPlaybackDate:(NSDate*)date {
  [_nowPlayingInfo setObject:date
                      forKey:MPNowPlayingInfoPropertyCurrentPlaybackDate];
}

- (void)setElapsedPlaybackTime:(NSNumber*)time {
  [_nowPlayingInfo setObject:time
                      forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
}

- (void)setDuration:(NSNumber*)duration {
  [_nowPlayingInfo setObject:duration
                      forKey:MPMediaItemPropertyPlaybackDuration];
}

- (void)setThumbnail:(NSImage*)image {
  if (@available(macOS 10.13.2, *)) {
    base::scoped_nsobject<MPMediaItemArtwork> artwork(
        [[MPMediaItemArtwork alloc]
            initWithBoundsSize:image.size
                requestHandler:^NSImage* _Nonnull(CGSize aSize) {
                  return image;
                }]);
    [_nowPlayingInfo setObject:artwork forKey:MPMediaItemPropertyArtwork];
  }
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
  [_nowPlayingInfo setObject:@"" forKey:MPMediaItemPropertyTitle];
  [_nowPlayingInfo setObject:@"" forKey:MPMediaItemPropertyArtist];
  [_nowPlayingInfo setObject:@"" forKey:MPMediaItemPropertyAlbumTitle];
  if (@available(macOS 10.13.2, *)) {
    [_nowPlayingInfo removeObjectForKey:MPMediaItemPropertyArtwork];
  }
}

- (void)updateNowPlayingInfo {
  [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = _nowPlayingInfo;
}

@end
