// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_FULLSCREEN_FULLSCREEN_TOOLBAR_MOUSE_TRACKER_H_
#define CHROME_BROWSER_UI_COCOA_FULLSCREEN_FULLSCREEN_TOOLBAR_MOUSE_TRACKER_H_

#import <Cocoa/Cocoa.h>

@class FullscreenToolbarController;

// Class that tracks mouse interactions with the fullscreen toolbar.
@interface FullscreenToolbarMouseTracker : NSObject

// Designated initializer.
- (instancetype)initWithFullscreenToolbarController:
    (FullscreenToolbarController*)owner;

// Updates the tracking area's frame to the given toolbar frame.
- (void)updateToolbarFrame:(NSRect)frame;

// Updates the tracking area. Remove it if the toolbar isn't fully shown.
- (void)updateTrackingArea;

// Removes the tracking area.
- (void)removeTrackingArea;

@end

#endif  // CHROME_BROWSER_UI_COCOA_FULLSCREEN_FULLSCREEN_TOOLBAR_MOUSE_TRACKER_H_
