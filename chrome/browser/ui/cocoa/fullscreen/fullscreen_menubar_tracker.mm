// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_menubar_tracker.h"

#include <Carbon/Carbon.h>
#include <QuartzCore/QuartzCore.h>

#include "base/mac/mac_util.h"
#include "base/macros.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_visibility_lock_controller.h"
#include "ui/base/cocoa/appkit_utils.h"

namespace {

// The event kind value for a undocumented menubar show/hide Carbon event.
const CGFloat kMenuBarRevealEventKind = 2004;

OSStatus MenuBarRevealHandler(EventHandlerCallRef handler,
                              EventRef event,
                              void* context) {
  FullscreenMenubarTracker* self =
      static_cast<FullscreenMenubarTracker*>(context);

  // If Chrome has multiple fullscreen windows in their own space, the Handler
  // becomes flaky and might start receiving kMenuBarRevealEventKind events
  // from another space. Since the menubar in the another space is in either a
  // shown or hidden state, it will give us a reveal fraction of 0.0 or 1.0.
  // As such, we should ignore the kMenuBarRevealEventKind event if it gives
  // us a fraction of 0.0 or 1.0, and rely on kEventMenuBarShown and
  // kEventMenuBarHidden to set these values.
  if (GetEventKind(event) == kMenuBarRevealEventKind) {
    CGFloat revealFraction = 0;
    GetEventParameter(event, FOUR_CHAR_CODE('rvlf'), typeCGFloat, NULL,
                      sizeof(CGFloat), NULL, &revealFraction);
    if (revealFraction > 0.0 && revealFraction < 1.0)
      [self setMenubarProgress:revealFraction];
  } else if (GetEventKind(event) == kEventMenuBarShown) {
    [self setMenubarProgress:1.0];
  } else {
    [self setMenubarProgress:0.0];
  }

  return CallNextEventHandler(handler, event);
}

}  // end namespace

@interface FullscreenMenubarTracker () {
  FullscreenToolbarController* controller_;        // weak
  id<FullscreenToolbarContextDelegate> delegate_;  // weak

  // A Carbon event handler that tracks the revealed fraction of the menubar.
  EventHandlerRef menubarTrackingHandler_;
}

// Returns YES if the mouse is on the same screen as the window.
- (BOOL)isMouseOnScreen;

@end

@implementation FullscreenMenubarTracker

@synthesize state = state_;
@synthesize menubarFraction = menubarFraction_;

- (instancetype)initWithFullscreenToolbarController:
    (FullscreenToolbarController*)controller {
  if ((self = [super init])) {
    controller_ = controller;
    delegate_ = [controller delegate];
    state_ = FullscreenMenubarState::HIDDEN;

    // Install the Carbon event handler for the menubar show, hide and
    // undocumented reveal event.
    EventTypeSpec eventSpecs[3];

    eventSpecs[0].eventClass = kEventClassMenu;
    eventSpecs[0].eventKind = kMenuBarRevealEventKind;

    eventSpecs[1].eventClass = kEventClassMenu;
    eventSpecs[1].eventKind = kEventMenuBarShown;

    eventSpecs[2].eventClass = kEventClassMenu;
    eventSpecs[2].eventKind = kEventMenuBarHidden;

    InstallApplicationEventHandler(NewEventHandlerUPP(&MenuBarRevealHandler),
                                   arraysize(eventSpecs), eventSpecs, self,
                                   &menubarTrackingHandler_);

    // Register for Active Space change notifications.
    [[[NSWorkspace sharedWorkspace] notificationCenter]
        addObserver:self
           selector:@selector(activeSpaceDidChange:)
               name:NSWorkspaceActiveSpaceDidChangeNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  RemoveEventHandler(menubarTrackingHandler_);
  [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];

  [super dealloc];
}

- (CGFloat)menubarFraction {
  return menubarFraction_;
}

- (void)setMenubarProgress:(CGFloat)progress {
  if (![delegate_ isInAnyFullscreenMode] ||
      [delegate_ isFullscreenTransitionInProgress]) {
    return;
  }

  // If the menubarFraction increases, check if we are in the right screen
  // so that the toolbar is not revealed on the wrong screen.
  if (![self isMouseOnScreen] && progress > menubarFraction_)
    return;

  // Ignore the menubarFraction changes if the Space is inactive.
  if (![[delegate_ window] isOnActiveSpace])
    return;

  if (ui::IsCGFloatEqual(progress, 1.0))
    state_ = FullscreenMenubarState::SHOWN;
  else if (ui::IsCGFloatEqual(progress, 0.0))
    state_ = FullscreenMenubarState::HIDDEN;
  else if (progress < menubarFraction_)
    state_ = FullscreenMenubarState::HIDING;
  else if (progress > menubarFraction_)
    state_ = FullscreenMenubarState::SHOWING;

  menubarFraction_ = progress;
  [controller_ layoutToolbar];

  // AppKit drives the menu bar animation from a nested run loop. Flush
  // explicitly so that Chrome's UI updates during the animation.
  [CATransaction flush];
}

- (BOOL)isMouseOnScreen {
  return NSMouseInRect([NSEvent mouseLocation],
                       [[delegate_ window] screen].frame, false);
}

- (void)activeSpaceDidChange:(NSNotification*)notification {
  menubarFraction_ = 0.0;
  state_ = FullscreenMenubarState::HIDDEN;
  [[controller_ visibilityLockController] releaseToolbarVisibilityForOwner:self
                                                             withAnimation:NO];
  [controller_ layoutToolbar];
}

@end
