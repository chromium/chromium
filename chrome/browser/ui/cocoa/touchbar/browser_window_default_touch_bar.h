// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TOUCHBAR_BROWSER_WINDOW_DEFAULT_TOUCH_BAR_H_
#define CHROME_BROWSER_UI_COCOA_TOUCHBAR_BROWSER_WINDOW_DEFAULT_TOUCH_BAR_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/availability.h"
#import "ui/base/cocoa/touch_bar_forward_declarations.h"

class BookmarkTabHelperObserver;
class Browser;
@class BrowserWindowTouchBarController;

// Provides a default touch bar for the browser window. This class implements
// the NSTouchBarDelegate and handles the items in the touch bar.
API_AVAILABLE(macos(10.12.2))
@interface BrowserWindowDefaultTouchBar : NSObject<NSTouchBarDelegate>
// True is the current page is loading. Used to determine if a stop or reload
// button should be provided.
@property(nonatomic, assign) BOOL isPageLoading;

// True if the current page is starred. Used by star touch bar button.
@property(nonatomic, assign) BOOL isStarred;

// True if the back button is enabled.
@property(nonatomic, assign) BOOL canGoBack;

// True if the forward button is enabled.
@property(nonatomic, assign) BOOL canGoForward;

@property(nonatomic, assign) BrowserWindowTouchBarController* controller;

@property(nonatomic) Browser* browser;

// Creates and returns a touch bar for the browser window.
- (NSTouchBar*)makeTouchBar;

- (BrowserWindowTouchBarController*)controller;

@end

// Private methods exposed for testing.
@interface BrowserWindowDefaultTouchBar (ExposedForTesting)

@property(readonly, class) NSString* reloadOrStopItemIdentifier;
@property(readonly, class) NSString* backItemIdentifier;
@property(readonly, class) NSString* forwardItemIdentifier;
@property(readonly, class) NSString* fullscreenOriginItemIdentifier;

// Updates the reload/stop button. Called when creating the touch bar or the
// page load state has been updated.
- (void)updateReloadStopButton;

// Returns the reload/stop button on the touch bar. Creates it if it's null.
- (NSButton*)reloadStopButton;

// Returns the bridge object that BrowserWindowDefaultTouchBar uses to receive
// notifications.
- (BookmarkTabHelperObserver*)bookmarkTabObserver;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TOUCHBAR_BROWSER_WINDOW_DEFAULT_TOUCH_BAR_H_
