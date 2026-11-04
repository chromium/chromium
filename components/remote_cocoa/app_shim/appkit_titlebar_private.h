// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_APPKIT_TITLEBAR_PRIVATE_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_APPKIT_TITLEBAR_PRIVATE_H_

#import <AppKit/AppKit.h>

// Private AppKit API used by immersive fullscreen v2. See
// ImmersiveModeTransparentTitlebarControllerCocoa.

@interface NSTitlebarAccessoryViewController (Private)
// Ranges from [0, 1]. Set by AppKit for every step of the fullscreen reveal
// animation.
@property(nonatomic) double revealAmount;
@end

@interface NSView (TitlebarPrivate)
// Implemented by NSTitlebarContainerView. Backs
// -[NSWindow setTitlebarAppearsTransparentWhenFullscreen:]. When YES, the
// titlebar keeps honoring `titlebarAppearsTransparent` while it is hosted in
// the NSToolbarFullScreenWindow.
@property(nonatomic) BOOL transparencyAllowedInFullScreen;

// Implemented by NSTitlebarView. Re-evaluates whether the titlebar background
// should be drawn.
- (void)_updateSectionTransparency;
@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_APPKIT_TITLEBAR_PRIVATE_H_
