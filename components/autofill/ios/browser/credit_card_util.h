// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_CREDIT_CARD_UTIL_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_CREDIT_CARD_UTIL_H_

#import <Foundation/Foundation.h>

#include <string>

namespace autofill {

class CreditCard;

// Returns |credit_card| name in |locale| as an autoreleased NSString.
NSString* GetCreditCardName(const CreditCard& credit_card,
                            const std::string& locale);

// Returns |credit_card| card identifier string as an autoreleased NSString.
NSString* GetCreditCardNameAndLastFourDigits(const CreditCard& credit_card);

// Returns |credit_card| nickname string as an autoreleased NSString.
NSString* GetCreditCardNicknameString(const CreditCard& credit_card);

// Returns |credit_card| expiration date as an autoreleased NSDateComponents.
// Only |year| and |month| fields of the NSDateComponents are valid.
NSDateComponents* GetCreditCardExpirationDate(const CreditCard& credit_card);

// Returns whether |credit_card| is a local card.
BOOL IsCreditCardLocal(const CreditCard& credit_card);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_CREDIT_CARD_UTIL_H_
