// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_NOW_PLAYING_INFO_CENTER_DELEGATE_COCOA_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_NOW_PLAYING_INFO_CENTER_DELEGATE_COCOA_H_

#import <Cocoa/Cocoa.h>
#import <MediaPlayer/MediaPlayer.h>

@interface NowPlayingInfoCenterDelegateCocoa : NSObject

- (instancetype)init;

// Clears all "Now Playing" information.
- (void)resetNowPlayingInfo;

// Called by the NowPlayingInfoCenterDelegateImpl to set metadata.
- (void)setPlaybackState:(MPNowPlayingPlaybackState)state;
- (void)setTitle:(NSString*)title;
- (void)setArtist:(NSString*)artist;
- (void)setAlbum:(NSString*)album;
- (void)setPlaybackRate:(NSNumber*)rate;
- (void)setCurrentPlaybackDate:(NSDate*)date;
- (void)setElapsedPlaybackTime:(NSNumber*)time;
- (void)setDuration:(NSNumber*)duration;
- (void)setThumbnail:(NSImage*)image;
- (void)updateNowPlayingInfo;

// Sets all metadata to default values.
- (void)clearMetadata;

@end

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_NOW_PLAYING_INFO_CENTER_DELEGATE_COCOA_H_
