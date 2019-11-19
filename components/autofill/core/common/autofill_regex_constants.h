// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_REGEX_CONSTANTS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_REGEX_CONSTANTS_H_

namespace autofill {

extern const char kAttentionIgnoredRe[];
extern const char kRegionIgnoredRe[];
extern const char kAddressNameIgnoredRe[];
extern const char kCompanyRe[];
extern const char kAddressLine1Re[];
extern const char kAddressLine1LabelRe[];
extern const char kAddressLine2Re[];
extern const char kAddressLine2LabelRe[];
extern const char kAddressLinesExtraRe[];
extern const char kAddressLookupRe[];
extern const char kCountryRe[];
extern const char kCountryLocationRe[];
extern const char kZipCodeRe[];
extern const char kZip4Re[];
extern const char kCityRe[];
extern const char kStateRe[];
extern const char kNameOnCardRe[];
extern const char kNameOnCardContextualRe[];
extern const char kCardNumberRe[];
extern const char kCardCvcRe[];
extern const char kCardTypeRe[];
extern const char kExpirationMonthRe[];
extern const char kExpirationYearRe[];
extern const char kExpirationDate2DigitYearRe[];
extern const char kExpirationDate4DigitYearRe[];
extern const char kExpirationDateRe[];
extern const char kCardIgnoredRe[];
extern const char kGiftCardRe[];
extern const char kDebitGiftCardRe[];
extern const char kDebitCardRe[];
extern const char kEmailRe[];
extern const char kNameIgnoredRe[];
extern const char kNameRe[];
extern const char kNameSpecificRe[];
extern const char kFirstNameRe[];
extern const char kMiddleInitialRe[];
extern const char kMiddleNameRe[];
extern const char kLastNameRe[];
extern const char kPhoneRe[];
extern const char kCountryCodeRe[];
extern const char kAreaCodeNotextRe[];
extern const char kAreaCodeRe[];
extern const char kFaxRe[];
extern const char kPhonePrefixSeparatorRe[];
extern const char kPhoneSuffixSeparatorRe[];
extern const char kPhonePrefixRe[];
extern const char kPhoneSuffixRe[];
extern const char kPhoneExtensionRe[];
extern const char kSearchTermRe[];
extern const char kPassportRe[];
extern const char kTravelOriginRe[];
extern const char kTravelDestinationRe[];
extern const char kFlightRe[];
extern const char kPriceRe[];
extern const char kCreditCardCVCPattern[];
extern const char kCreditCard4DigitExpYearPattern[];

// Used to match field data that might be a UPI Virtual Payment Address.
// See:
//   - http://crbug.com/702220
//   - https://upipayments.co.in/virtual-payment-address-vpa/
extern const char kUPIVirtualPaymentAddressRe[];

// Used to match field data that might be an International Bank Account Number.
// TODO(crbug.com/977377): The regex doesn't match IBANs for Saint Lucia (LC),
// Kazakhstan (KZ) and Romania (RO). Consider replace the regex with something
// like "(?:IT|SM)\d{2}[A-Z]\d{22}|CY\d{2}[A-Z]\d{23}...". For reference:
//    - https://www.swift.com/resource/iban-registry-pdf
extern const char kInternationalBankAccountNumberRe[];

// Match the path values for form actions that look like generic search:
//  e.g. /search
//       /search/
//       /search/products...
//       /products/search/
//       /blah/search_all.jsp
extern const char kUrlSearchActionRe[];

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_REGEX_CONSTANTS_H_
