// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/touchbar/browser_window_touch_bar_controller.h"

#include <memory>

#include "base/mac/mac_util.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/touchbar/browser_window_default_touch_bar.h"
#import "chrome/browser/ui/cocoa/touchbar/web_textfield_touch_bar_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#import "ui/base/cocoa/touch_bar_util.h"

@interface BrowserWindowTouchBarController () {
  NSWindow* __weak _window;

  BrowserWindowDefaultTouchBar* __strong _defaultTouchBar;

  WebTextfieldTouchBarController* __strong _webTextfieldTouchBar;
}
@end

@implementation BrowserWindowTouchBarController

- (instancetype)initWithBrowser:(Browser*)browser window:(NSWindow*)window {
  if ((self = [super init])) {
    DCHECK(browser);
    _window = window;

    _defaultTouchBar = [[BrowserWindowDefaultTouchBar alloc] init];
    _defaultTouchBar.controller = self;
    _defaultTouchBar.browser = browser;
    _webTextfieldTouchBar =
        [[WebTextfieldTouchBarController alloc] initWithController:self];
  }

  return self;
}

- (void)dealloc {
  _defaultTouchBar.browser = nullptr;
}

- (void)invalidateTouchBar {
  _window.touchBar = nil;
}

- (NSTouchBar*)makeTouchBar {
  NSTouchBar* touchBar = [_webTextfieldTouchBar makeTouchBar];
  if (touchBar)
    return touchBar;

  return [_defaultTouchBar makeTouchBar];
}

@end

@implementation BrowserWindowTouchBarController (ExposedForTesting)

- (BrowserWindowDefaultTouchBar*)defaultTouchBar {
  return _defaultTouchBar;
}

- (WebTextfieldTouchBarController*)webTextfieldTouchBar {
  return _webTextfieldTouchBar;
}

@end
