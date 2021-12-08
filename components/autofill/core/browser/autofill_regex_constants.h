// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_REGEX_CONSTANTS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_REGEX_CONSTANTS_H_

namespace autofill {

extern const char16_t kAttentionIgnoredRe[];
extern const char16_t kRegionIgnoredRe[];
extern const char16_t kAddressNameIgnoredRe[];
extern const char16_t kCompanyRe[];
extern const char16_t kHouseNumberRe[];
extern const char16_t kApartmentNumberRe[];
extern const char16_t kStreetNameRe[];
extern const char16_t kAddressLine1Re[];
extern const char16_t kAddressLine1LabelRe[];
extern const char16_t kAddressLine2Re[];
extern const char16_t kAddressLine2LabelRe[];
extern const char16_t kAddressLinesExtraRe[];
extern const char16_t kAddressLookupRe[];
extern const char16_t kCountryRe[];
extern const char16_t kDependentLocality[];
extern const char16_t kCountryLocationRe[];
extern const char16_t kZipCodeRe[];
extern const char16_t kZip4Re[];
extern const char16_t kDependentLocalityRe[];
extern const char16_t kCityRe[];
extern const char16_t kStateRe[];
extern const char16_t kNameOnCardRe[];
extern const char16_t kNameOnCardContextualRe[];
extern const char16_t kCardNumberRe[];
extern const char16_t kCardCvcRe[];
extern const char16_t kCardTypeRe[];
extern const char16_t kExpirationMonthRe[];
extern const char16_t kExpirationYearRe[];
extern const char16_t kExpirationDate2DigitYearRe[];
extern const char16_t kExpirationDate4DigitYearRe[];
extern const char16_t kExpirationDateRe[];
extern const char16_t kCardIgnoredRe[];
extern const char16_t kGiftCardRe[];
extern const char16_t kDebitGiftCardRe[];
extern const char16_t kDebitCardRe[];
extern const char16_t kDayRe[];
extern const char16_t kEmailRe[];
extern const char16_t kNameIgnoredRe[];
extern const char16_t kFullNameRe[];
extern const char16_t kNameGenericRe[];
extern const char16_t kFirstNameRe[];
extern const char16_t kMiddleInitialRe[];
extern const char16_t kMiddleNameRe[];
extern const char16_t kLastNameRe[];
extern const char16_t kHonorificPrefixRe[];
extern const char16_t kNameLastFirstRe[];
extern const char16_t kNameLastSecondRe[];
extern const char16_t kPhoneRe[];
extern const char16_t kAugmentedPhoneCountryCodeRe[];
extern const char16_t kCountryCodeRe[];
extern const char16_t kAreaCodeNotextRe[];
extern const char16_t kAreaCodeRe[];
extern const char16_t kFaxRe[];
extern const char16_t kPhonePrefixSeparatorRe[];
extern const char16_t kPhoneSuffixSeparatorRe[];
extern const char16_t kPhonePrefixRe[];
extern const char16_t kPhoneSuffixRe[];
extern const char16_t kPhoneExtensionRe[];
extern const char16_t kSearchTermRe[];
extern const char16_t kPassportRe[];
extern const char16_t kTravelOriginRe[];
extern const char16_t kTravelDestinationRe[];
extern const char16_t kFlightRe[];
extern const char16_t kPriceRe[];
extern const char16_t kCreditCardCVCPattern[];
extern const char16_t kCreditCard4DigitExpYearPattern[];
extern const char16_t kSocialSecurityRe[];
extern const char16_t kOneTimePwdRe[];
extern const char16_t kHiddenValueRe[];
extern const char16_t kMerchantPromoCodeRe[];
extern const char16_t kEmailValueRe[];
extern const char16_t kPhoneValueRe[];
extern const char16_t kUsernameLikeValueRe[];

// Used to match field data that might be a UPI Virtual Payment Address.
// See:
//   - http://crbug.com/702220
//   - https://upipayments.co.in/virtual-payment-address-vpa/
extern const char16_t kUPIVirtualPaymentAddressRe[];

// Used to match field data that might be an International Bank Account Number.
// TODO(crbug.com/977377): The regex doesn't match IBANs for Saint Lucia (LC),
// Kazakhstan (KZ) and Romania (RO). Consider replace the regex with something
// like "(?:IT|SM)\d{2}[A-Z]\d{22}|CY\d{2}[A-Z]\d{23}...". For reference:
//    - https://www.swift.com/resource/iban-registry-pdf
extern const char16_t kInternationalBankAccountNumberRe[];

// Match the path values for form actions that look like generic search:
//  e.g. /search
//       /search/
//       /search/products...
//       /products/search/
//       /blah/search_all.jsp
extern const char16_t kUrlSearchActionRe[];

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_REGEX_CONSTANTS_H_
