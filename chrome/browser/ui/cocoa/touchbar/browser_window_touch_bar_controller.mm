// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/touchbar/browser_window_touch_bar_controller.h"

#include <memory>

#include "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/touchbar/browser_window_default_touch_bar.h"
#import "chrome/browser/ui/cocoa/touchbar/web_textfield_touch_bar_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#import "ui/base/cocoa/touch_bar_util.h"

@interface BrowserWindowTouchBarController () {
  NSWindow* _window;  // Weak.

  base::scoped_nsobject<BrowserWindowDefaultTouchBar> _defaultTouchBar;

  base::scoped_nsobject<WebTextfieldTouchBarController> _webTextfieldTouchBar;
}
@end

@implementation BrowserWindowTouchBarController

- (instancetype)initWithBrowser:(Browser*)browser window:(NSWindow*)window {
  if ((self = [super init])) {
    DCHECK(browser);
    _window = window;

    _defaultTouchBar.reset([[BrowserWindowDefaultTouchBar alloc] init]);
    _defaultTouchBar.get().controller = self;
    _defaultTouchBar.get().browser = browser;
    _webTextfieldTouchBar.reset(
        [[WebTextfieldTouchBarController alloc] initWithController:self]);
  }

  return self;
}

- (void)dealloc {
  _defaultTouchBar.get().browser = nullptr;
  [super dealloc];
}

- (void)invalidateTouchBar {
  DCHECK([_window respondsToSelector:@selector(setTouchBar:)]);
  [_window performSelector:@selector(setTouchBar:) withObject:nil];
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
  return _defaultTouchBar.get();
}

- (WebTextfieldTouchBarController*)webTextfieldTouchBar {
  return _webTextfieldTouchBar.get();
}

@end
