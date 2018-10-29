// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/touchbar/web_textfield_touch_bar_controller.h"

#include "base/debug/stack_trace.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#import "chrome/browser/ui/cocoa/touchbar/browser_window_touch_bar_controller.h"
#import "chrome/browser/ui/cocoa/touchbar/credit_card_autofill_touch_bar_controller.h"
#import "chrome/browser/ui/cocoa/touchbar/text_suggestions_touch_bar_controller.h"
#include "chrome/browser/ui/views/frame/browser_frame_mac.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#import "ui/base/cocoa/touch_bar_util.h"
#include "ui/base/ui_base_features.h"

@implementation WebTextfieldTouchBarController

+ (WebTextfieldTouchBarController*)controllerForWindow:(NSWindow*)window {
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(window);
  if (!browser_view)
    return nil;

  BrowserFrameMac* browser_frame = static_cast<BrowserFrameMac*>(
      browser_view->frame()->native_browser_frame());
  return [browser_frame->GetTouchBarController() webTextfieldTouchBar];
}

- (instancetype)initWithController:
    (BrowserWindowTouchBarController*)controller {
  if ((self = [super init])) {
    controller_ = controller;

    if (base::FeatureList::IsEnabled(features::kTextSuggestionsTouchBar) ||
        base::FeatureList::IsEnabled(features::kExperimentalUi)) {
      textSuggestionsTouchBarController_.reset(
          [[TextSuggestionsTouchBarController alloc]
              initWithWebContents:[controller_ webContents]
                       controller:self]);
    }
  }

  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)showCreditCardAutofillWithController:
    (autofill::AutofillPopupController*)controller {
  autofillTouchBarController_.reset(
      [[CreditCardAutofillTouchBarController alloc]
          initWithController:controller]);
  [self invalidateTouchBar];
}

- (void)hideCreditCardAutofillTouchBar {
  autofillTouchBarController_.reset();
  [self invalidateTouchBar];
}

- (void)updateWebContents:(content::WebContents*)contents {
  [textSuggestionsTouchBarController_ setWebContents:contents];
}

- (void)invalidateTouchBar {
  [controller_ invalidateTouchBar];
}

- (NSTouchBar*)makeTouchBar {
  if (autofillTouchBarController_)
    return [autofillTouchBarController_ makeTouchBar];

  if (textSuggestionsTouchBarController_)
    return [textSuggestionsTouchBarController_ makeTouchBar];

  return nil;
}

@end
