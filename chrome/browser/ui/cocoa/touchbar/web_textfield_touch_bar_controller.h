// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TOUCHBAR_WEB_TEXTFIELD_TOUCH_BAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_TOUCHBAR_WEB_TEXTFIELD_TOUCH_BAR_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include <memory>

#import "base/mac/scoped_nsobject.h"
#import "ui/base/cocoa/touch_bar_forward_declarations.h"

@class BrowserWindowTouchBarController;
@class CreditCardAutofillTouchBarController;
@class TextSuggestionsTouchBarController;

namespace autofill {
class AutofillPopupController;
}

namespace content {
class WebContents;
}

// Provides a touch bar for the textfields in the WebContents. This class
// implements the NSTouchBarDelegate and handles the items in the touch bar.
API_AVAILABLE(macos(10.12.2))
@interface WebTextfieldTouchBarController : NSObject<NSTouchBarDelegate> {
  BrowserWindowTouchBarController* controller_;  // weak.
  base::scoped_nsobject<CreditCardAutofillTouchBarController>
      autofillTouchBarController_;
  base::scoped_nsobject<TextSuggestionsTouchBarController>
      textSuggestionsTouchBarController_;
}

+ (WebTextfieldTouchBarController*)controllerForWindow:(NSWindow*)window;

// Designated initializer.
- (instancetype)initWithController:(BrowserWindowTouchBarController*)controller;

- (void)showCreditCardAutofillWithController:
    (autofill::AutofillPopupController*)controller;

- (void)hideCreditCardAutofillTouchBar;

- (void)updateWebContents:(content::WebContents*)contents;

- (void)invalidateTouchBar;

// Creates and returns a touch bar.
- (NSTouchBar*)makeTouchBar;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TOUCHBAR_WEB_TEXTFIELD_TOUCH_BAR_CONTROLLER_H_
