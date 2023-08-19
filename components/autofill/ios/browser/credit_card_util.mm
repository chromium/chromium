// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/browser/credit_card_util.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

namespace autofill {

NSString* GetCreditCardName(const CreditCard& credit_card,
                            const std::string& locale) {
  return base::SysUTF16ToNSString(credit_card.GetInfo(
      autofill::AutofillType(autofill::CREDIT_CARD_NAME_FULL), locale));
}

NSString* GetCreditCardNameAndLastFourDigits(const CreditCard& credit_card) {
  return base::SysUTF16ToNSString(credit_card.CardNameAndLastFourDigits());
}

NSString* GetCreditCardNicknameString(const CreditCard& credit_card) {
  return base::SysUTF16ToNSString(credit_card.nickname());
}

NSDateComponents* GetCreditCardExpirationDate(const CreditCard& credit_card) {
  NSDateComponents* expiration_date = [[NSDateComponents alloc] init];
  expiration_date.year = credit_card.expiration_year();
  expiration_date.month = credit_card.expiration_month();
  return expiration_date;
}

BOOL IsCreditCardLocal(const CreditCard& credit_card) {
  return credit_card.record_type() ==
         autofill::CreditCard::RecordType::kLocalCard;
}

}  // namespace autofill
