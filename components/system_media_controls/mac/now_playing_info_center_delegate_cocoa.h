// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_NOW_PLAYING_INFO_CENTER_DELEGATE_COCOA_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_NOW_PLAYING_INFO_CENTER_DELEGATE_COCOA_H_

#import <Cocoa/Cocoa.h>
#import <MediaPlayer/MediaPlayer.h>

API_AVAILABLE(macos(10.12.2))
@interface NowPlayingInfoCenterDelegateCocoa : NSObject

- (instancetype)init;

// Clears all "Now Playing" information.
- (void)resetNowPlayingInfo;

// Called by the NowPlayingInfoCenterDelegateImpl to set metadata.
- (void)setPlaybackState:(MPNowPlayingPlaybackState)state;
- (void)setTitle:(NSString*)title;
- (void)setArtist:(NSString*)artist;
- (void)setAlbum:(NSString*)album;

// Sets all metadata to default values.
- (void)clearMetadata;

@end

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_NOW_PLAYING_INFO_CENTER_DELEGATE_COCOA_H_
