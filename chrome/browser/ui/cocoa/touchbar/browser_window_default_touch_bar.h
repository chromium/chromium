// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TOUCHBAR_BROWSER_WINDOW_DEFAULT_TOUCH_BAR_H_
#define CHROME_BROWSER_UI_COCOA_TOUCHBAR_BROWSER_WINDOW_DEFAULT_TOUCH_BAR_H_

#import <Cocoa/Cocoa.h>

class BookmarkTabHelperObserver;
class Browser;
@class BrowserWindowTouchBarController;

// Provides a default touch bar for the browser window. This class implements
// the NSTouchBarDelegate and handles the items in the touch bar.
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
@property(readonly, class) NSString* bookmarkStarItemIdentifier;
@property(readonly, class) NSString* backItemIdentifier;
@property(readonly, class) NSString* forwardItemIdentifier;
@property(readonly, class) NSString* fullscreenOriginItemIdentifier;
@property(readonly, class) NSImage* starDefaultIcon;
@property(readonly, class) NSImage* starActiveIcon;
@property(readonly, class) NSImage* navigateStopIcon;
@property(readonly, class) NSImage* reloadIcon;
@property(readonly, class) NSString* homeItemIdentifier;

- (NSButton*)searchButton;

// Returns the bridge object that BrowserWindowDefaultTouchBar uses to receive
// notifications.
- (BookmarkTabHelperObserver*)bookmarkTabObserver;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TOUCHBAR_BROWSER_WINDOW_DEFAULT_TOUCH_BAR_H_
