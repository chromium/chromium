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

// For testing, a block that can be set to mock the return value of
// `isKeyDownForKeyCode`. Used to simulate the pressing/holding of the quit
// accelerator.
@property(class, copy) BOOL (^isKeyDownForKeyCodeMock)(unsigned short);

// Displays the "Hold to Quit" HUD and runs a nested event loop to determine
// whether the application should terminate. This implements both the
// "Hold to Quit" and "Double-tap to Quit" behaviors. Returns YES if the quit
// should proceed.
//
// |event| is the KeyDown event that triggered the quit attempt; it is used to
// identify the key being held (typically 'Q') without hardcoding its key code.
- (BOOL)runConfirmQuitLoopWithEvent:(NSEvent*)event;

// Shows the window.
- (void)showWindow:(id)sender;

// If the user did not confirm quit, send this message to give the user
// instructions on how to quit.
- (void)dismissPanel;

// Completely back out of the process, making all windows visible again.
// Called when the quit was aborted *after* confirmations (for example, due to
// pending downloads or `beforeunload`).
- (void)cancel;

@end

#endif  // CHROME_BROWSER_UI_COCOA_CONFIRM_QUIT_PANEL_CONTROLLER_H_
