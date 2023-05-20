// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TOUCHBAR_CREDIT_CARD_AUTOFILL_TOUCH_BAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_TOUCHBAR_CREDIT_CARD_AUTOFILL_TOUCH_BAR_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

namespace autofill {
class AutofillPopupController;
}

@interface CreditCardAutofillTouchBarController : NSObject <NSTouchBarDelegate>

- (instancetype)initWithController:
    (autofill::AutofillPopupController*)controller;

- (NSTouchBar*)makeTouchBar;

- (NSTouchBarItem*)touchBar:(NSTouchBar*)touchBar
      makeItemForIdentifier:(NSTouchBarItemIdentifier)identifier;

@end

@interface CreditCardAutofillTouchBarController (ExposedForTesting)

- (NSButton*)createCreditCardButtonAtRow:(int)row;
- (void)acceptCreditCard:(id)sender;
- (void)setIsCreditCardPopup:(bool)is_credit_card_popup;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TOUCHBAR_CREDIT_CARD_AUTOFILL_TOUCH_BAR_CONTROLLER_H_
