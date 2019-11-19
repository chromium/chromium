// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/browser/credit_card_util.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

NSString* GetCreditCardName(const CreditCard& credit_card,
                            const std::string& locale) {
  return base::SysUTF16ToNSString(credit_card.GetInfo(
      autofill::AutofillType(autofill::CREDIT_CARD_NAME_FULL), locale));
}

NSString* GetCreditCardObfuscatedNumber(const CreditCard& credit_card) {
  return base::SysUTF16ToNSString(
      credit_card.NetworkOrBankNameAndLastFourDigits());
}

NSDateComponents* GetCreditCardExpirationDate(const CreditCard& credit_card) {
  NSDateComponents* expiration_date = [[NSDateComponents alloc] init];
  expiration_date.year = credit_card.expiration_year();
  expiration_date.month = credit_card.expiration_month();
  return expiration_date;
}

BOOL IsCreditCardLocal(const CreditCard& credit_card) {
  return credit_card.record_type() == autofill::CreditCard::LOCAL_CARD;
}

}  // namespace autofill
