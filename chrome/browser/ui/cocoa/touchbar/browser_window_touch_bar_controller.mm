// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/touchbar/browser_window_touch_bar_controller.h"

#include <memory>

#include "base/mac/availability.h"
#include "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/sdk_forward_declarations.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/touchbar/browser_window_default_touch_bar.h"
#import "chrome/browser/ui/cocoa/touchbar/web_textfield_touch_bar_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#import "ui/base/cocoa/touch_bar_util.h"

@interface BrowserWindowTouchBarController () {
  NSWindow* window_;  // Weak.

  base::scoped_nsobject<BrowserWindowDefaultTouchBar> defaultTouchBar_;

  base::scoped_nsobject<WebTextfieldTouchBarController> webTextfieldTouchBar_;
}
@end

@implementation BrowserWindowTouchBarController

- (instancetype)initWithBrowser:(Browser*)browser window:(NSWindow*)window {
  if ((self = [super init])) {
    DCHECK(browser);
    window_ = window;

    defaultTouchBar_.reset([[BrowserWindowDefaultTouchBar alloc] init]);
    defaultTouchBar_.get().controller = self;
    defaultTouchBar_.get().browser = browser;
    webTextfieldTouchBar_.reset(
        [[WebTextfieldTouchBarController alloc] initWithController:self]);
  }

  return self;
}

- (void)dealloc {
  defaultTouchBar_.get().browser = nullptr;
  [super dealloc];
}

- (void)invalidateTouchBar {
  DCHECK([window_ respondsToSelector:@selector(setTouchBar:)]);
  [window_ performSelector:@selector(setTouchBar:) withObject:nil];
}

- (NSTouchBar*)makeTouchBar {
  NSTouchBar* touchBar = [webTextfieldTouchBar_ makeTouchBar];
  if (touchBar)
    return touchBar;

  return [defaultTouchBar_ makeTouchBar];
}

@end

@implementation BrowserWindowTouchBarController (ExposedForTesting)

- (BrowserWindowDefaultTouchBar*)defaultTouchBar {
  return defaultTouchBar_.get();
}

- (WebTextfieldTouchBarController*)webTextfieldTouchBar {
  return webTextfieldTouchBar_.get();
}

@end
