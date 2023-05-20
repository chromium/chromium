// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TOUCHBAR_WEB_TEXTFIELD_TOUCH_BAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_TOUCHBAR_WEB_TEXTFIELD_TOUCH_BAR_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

@class BrowserWindowTouchBarController;
@class CreditCardAutofillTouchBarController;

namespace autofill {
class AutofillPopupController;
}

namespace content {
class WebContents;
}

// Provides a touch bar for the text fields in the WebContents. This class
// implements the NSTouchBarDelegate and handles the items in the touch bar.
@interface WebTextfieldTouchBarController : NSObject <NSTouchBarDelegate>

+ (WebTextfieldTouchBarController*)controllerForWindow:(NSWindow*)window;

// Designated initializer.
- (instancetype)initWithController:(BrowserWindowTouchBarController*)controller;

- (void)showCreditCardAutofillWithController:
    (autofill::AutofillPopupController*)controller;

- (void)hideCreditCardAutofillTouchBar;

- (void)invalidateTouchBar;

// Creates and returns a touch bar.
- (NSTouchBar*)makeTouchBar;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TOUCHBAR_WEB_TEXTFIELD_TOUCH_BAR_CONTROLLER_H_
