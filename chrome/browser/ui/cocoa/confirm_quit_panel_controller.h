// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CONFIRM_QUIT_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_CONFIRM_QUIT_PANEL_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

@class ConfirmQuitFrameView;

// The ConfirmQuitPanelController manages the black HUD window that tells users
// to "Hold Cmd+Q to Quit".
@interface ConfirmQuitPanelController : NSWindowController <NSWindowDelegate>

// Returns the singleton instance of the Controller. This will create one if it
// does not currently exist.
@property(class, readonly) ConfirmQuitPanelController* sharedController;

// Returns a string representation fit for display.
@property(class, readonly) NSString* keyCommandString;

// Runs a modal loop that brings up the panel and handles the logic for if and
// when to terminate. Returns YES if the quit should continue.
- (BOOL)runModalLoop;

// Shows the window.
- (void)showWindow:(id)sender;

// If the user did not confirm quit, send this message to give the user
// instructions on how to quit.
- (void)dismissPanel;

// Completely back out of the process, making all windows visible again.
// Called when the quit was aborted *after* confirmations (for example, due to
// pending downloads or `beforeunload`).
- (void)cancel;

// Hides windows and set state as if we had run `runModalLoop` and received
// a key up from the user.
- (void)simulateQuitForTesting;

@end

#endif  // CHROME_BROWSER_UI_COCOA_CONFIRM_QUIT_PANEL_CONTROLLER_H_
