// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/touchbar/web_textfield_touch_bar_controller.h"

#include "base/debug/stack_trace.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#import "chrome/browser/ui/cocoa/touchbar/browser_window_touch_bar_controller.h"
#import "chrome/browser/ui/cocoa/touchbar/credit_card_autofill_touch_bar_controller.h"
#include "chrome/browser/ui/views/frame/browser_frame_mac.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"
#import "ui/base/cocoa/touch_bar_util.h"

@implementation WebTextfieldTouchBarController {
  BrowserWindowTouchBarController* __weak _controller;
  CreditCardAutofillTouchBarController* __strong _autofillTouchBarController;
}

+ (WebTextfieldTouchBarController*)controllerForWindow:(NSWindow*)window {
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(window);
  if (!browser_view) {
    return nil;
  }

  BrowserFrameMac* browser_frame = static_cast<BrowserFrameMac*>(
      browser_view->frame()->native_browser_frame());
  return [browser_frame->GetTouchBarController() webTextfieldTouchBar];
}

- (instancetype)initWithController:
    (BrowserWindowTouchBarController*)controller {
  if ((self = [super init])) {
    _controller = controller;
  }

  return self;
}

- (void)dealloc {
  [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)showCreditCardAutofillWithController:
    (autofill::AutofillPopupController*)controller {
  _autofillTouchBarController = [[CreditCardAutofillTouchBarController alloc]
      initWithController:controller];
  [self invalidateTouchBar];
}

- (void)hideCreditCardAutofillTouchBar {
  _autofillTouchBarController = nil;
  [self invalidateTouchBar];
}

- (void)invalidateTouchBar {
  [_controller invalidateTouchBar];
}

- (NSTouchBar*)makeTouchBar {
  if (_autofillTouchBarController) {
    return [_autofillTouchBarController makeTouchBar];
  }
  return nil;
}

@end
