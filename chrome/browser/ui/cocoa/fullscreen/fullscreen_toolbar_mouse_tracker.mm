// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_mouse_tracker.h"

#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller.h"
#include "chrome/browser/ui/cocoa/scoped_menu_bar_lock.h"
#import "ui/base/cocoa/tracking_area.h"

namespace {

// Additional height threshold added at the toolbar's bottom. This is to mimic
// threshold the mouse position needs to be at before the menubar automatically
// hides.
const CGFloat kTrackingAreaAdditionalThreshold = 50;

}  // namespace

@interface FullscreenToolbarMouseTracker () {
  // The frame for the tracking area. The value is the toolbar's frame with
  // additional height added at the bottom.
  NSRect trackingAreaFrame_;

  // The tracking area associated with the toolbar. This tracking area is used
  // to keep the toolbar active if the menubar had animated out but the mouse
  // is still on the toolbar.
  base::scoped_nsobject<CrTrackingArea> trackingArea_;

  // Keeps the menu bar from hiding until the mouse exits the tracking area.
  std::unique_ptr<ScopedMenuBarLock> menuBarLock_;

  // The content view for the window.
  NSView* contentView_;  // weak

  FullscreenToolbarController* controller_;  // weak
}

@end

@implementation FullscreenToolbarMouseTracker

- (instancetype)initWithFullscreenToolbarController:
    (FullscreenToolbarController*)controller {
  if ((self = [super init])) {
    controller_ = controller;
  }

  return self;
}

- (void)dealloc {
  [self removeTrackingArea];
  [super dealloc];
}

- (void)updateTrackingArea {
  // Remove the tracking area if the toolbar and menu bar aren't both visible.
  if ([controller_ toolbarFraction] == 0 || ![NSMenu menuBarVisible]) {
    [self removeTrackingArea];
    menuBarLock_.reset();
    return;
  }

  if (trackingArea_) {
    // If |trackingArea_|'s rect matches |trackingAreaFrame_|, quit early.
    if (NSEqualRects(trackingAreaFrame_, [trackingArea_ rect]))
      return;

    [self removeTrackingArea];
  }

  contentView_ = [[[controller_ delegate] window] contentView];

  trackingArea_.reset([[CrTrackingArea alloc]
      initWithRect:trackingAreaFrame_
           options:NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow
             owner:self
          userInfo:nil]);

  [contentView_ addTrackingArea:trackingArea_];
}

- (void)updateToolbarFrame:(NSRect)frame {
  NSRect contentBounds = [[[[controller_ delegate] window] contentView] bounds];
  trackingAreaFrame_ = frame;
  trackingAreaFrame_.origin.y -= kTrackingAreaAdditionalThreshold;
  trackingAreaFrame_.size.height =
      NSMaxY(contentBounds) - trackingAreaFrame_.origin.y;

  [self updateTrackingArea];
}

- (void)removeTrackingArea {
  if (!trackingArea_)
    return;

  DCHECK(contentView_);
  [contentView_ removeTrackingArea:trackingArea_];
  trackingArea_.reset();
  contentView_ = nil;
}

- (void)mouseEntered:(NSEvent*)event {
  menuBarLock_ = std::make_unique<ScopedMenuBarLock>();
}

- (void)mouseExited:(NSEvent*)event {
  menuBarLock_.reset();
}

@end
