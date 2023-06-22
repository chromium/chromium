// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_FULLSCREEN_FULLSCREEN_MENUBAR_TRACKER_H_
#define CHROME_BROWSER_UI_COCOA_FULLSCREEN_FULLSCREEN_MENUBAR_TRACKER_H_

#import <Cocoa/Cocoa.h>

@class BrowserWindowController;
@class FullscreenToolbarController;

// State of the menubar in the window's screen.
enum class FullscreenMenubarState {
  SHOWN,    // Menubar is fully shown.
  HIDDEN,   // Menubar is fully hidden.
  SHOWING,  // Menubar is animating in.
  HIDING,   // Menubar is animating out.
};

@interface FullscreenMenubarTracker : NSObject

// The state of the menubar.
@property(nonatomic, readonly) FullscreenMenubarState state;

// The fraction of the menubar shown on the screen.
@property(nonatomic, readonly) CGFloat menubarFraction;

// Designated initializer.
- (instancetype)initWithFullscreenToolbarController:
    (FullscreenToolbarController*)owner;

// Called by MenuBarRevealHandler to update the menubar progress. The progress
// is only updated if the window is in fullscreen and the mouse is in the
// same screen.
- (void)setMenubarProgress:(CGFloat)progress;

@end

#endif  // CHROME_BROWSER_UI_COCOA_FULLSCREEN_FULLSCREEN_MENUBAR_TRACKER_H_
