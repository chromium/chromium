// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_REGEX_CONSTANTS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_REGEX_CONSTANTS_H_

namespace autofill {

// TODO(crbug.com/40280853): This regex corresponds to the
// "CREDIT_CARD_VERIFICATION_CODE" pattern. However, it has a use in password
// manager code, which doesn't use pattern provider. Either move this constant
// to password manager code or use the pattern provider pattern in
// password manager.
inline constexpr char16_t kCardCvcRe[] =
    u"verification|card.?identification|security.?code|card.?code"
    u"|security.?value"
    u"|security.?number|card.?pin|c-v-v"
    u"|código de segurança"  // pt-BR
    u"|código de seguridad"  // es-MX
    u"|karten.?prüfn"        // de-DE
    u"|(?:cvn|cvv|cvc|csc|cvd|ccv)"
    // We used to match "cid", but it is a substring of "cidade" (Portuguese for
    // "city") and needs to be handled carefully.
    u"|\\bcid\\b|cccid";

/////////////////////////////////////////////////////////////////////////////
// All regexes below this point are non-parsing related and thus don't have a
// JSON based definition.
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// data_model_utils.cc
/////////////////////////////////////////////////////////////////////////////
inline constexpr char16_t kAugmentedPhoneCountryCodeRe[] =
    u"^[^0-9+]*(?:\\+|00)\\s*([1-9]\\d{0,3})\\D*$";

/////////////////////////////////////////////////////////////////////////////
// validation.cc
/////////////////////////////////////////////////////////////////////////////

// Used to match field data that might be a UPI Virtual Payment Address.
// See:
//   - http://crbug.com/702220
//   - https://upipayments.co.in/virtual-payment-address-vpa/
inline constexpr char16_t kUPIVirtualPaymentAddressRe[] =
    u"^[\\w.+-_]+@("        // eg user@
    u"\\w+\\.ifsc\\.npci|"  // IFSC code
    u"aadhaar\\.npci|"      // Aadhaar number
    u"mobile\\.npci|"       // Mobile number
    u"rupay\\.npci|"        // RuPay card number
    u"airtel|"  // List of banks https://www.npci.org.in/upi-live-members
    u"airtelpaymentsbank|"
    u"albk|"
    u"allahabadbank|"
    u"allbank|"
    u"andb|"
    u"apb|"
    u"apl|"
    u"axis|"
    u"axisbank|"
    u"axisgo|"
    u"bandhan|"
    u"barodampay|"
    u"birla|"
    u"boi|"
    u"cbin|"
    u"cboi|"
    u"centralbank|"
    u"cmsidfc|"
    u"cnrb|"
    u"csbcash|"
    u"csbpay|"
    u"cub|"
    u"dbs|"
    u"dcb|"
    u"dcbbank|"
    u"denabank|"
    u"dlb|"
    u"eazypay|"
    u"equitas|"
    u"ezeepay|"
    u"fbl|"
    u"federal|"
    u"finobank|"
    u"hdfcbank|"
    u"hsbc|"
    u"icici|"
    u"idbi|"
    u"idbibank|"
    u"idfc|"
    u"idfcbank|"
    u"idfcnetc|"
    u"ikwik|"
    u"imobile|"
    u"indbank|"
    u"indianbank|"
    u"indianbk|"
    u"indus|"
    u"iob|"
    u"jkb|"
    u"jsb|"
    u"jsbp|"
    u"karb|"
    u"karurvysyabank|"
    u"kaypay|"
    u"kbl|"
    u"kbl052|"
    u"kmb|"
    u"kmbl|"
    u"kotak|"
    u"kvb|"
    u"kvbank|"
    u"lime|"
    u"lvb|"
    u"lvbank|"
    u"mahb|"
    u"obc|"
    u"okaxis|"
    u"okbizaxis|"
    u"okhdfcbank|"
    u"okicici|"
    u"oksbi|"
    u"paytm|"
    u"payzapp|"
    u"pingpay|"
    u"pnb|"
    u"pockets|"
    u"psb|"
    u"purz|"
    u"rajgovhdfcbank|"
    u"rbl|"
    u"sbi|"
    u"sc|"
    u"scb|"
    u"scbl|"
    u"scmobile|"
    u"sib|"
    u"srcb|"
    u"synd|"
    u"syndbank|"
    u"syndicate|"
    u"tjsb|"
    u"tjsp|"
    u"ubi|"
    u"uboi|"
    u"uco|"
    u"unionbank|"
    u"unionbankofindia|"
    u"united|"
    u"upi|"
    u"utbi|"
    u"vijayabank|"
    u"vijb|"
    u"vjb|"
    u"ybl|"
    u"yesbank|"
    u"yesbankltd"
    u")$";

// Used to match field value that might be an International Bank Account Number.
// TODO(crbug.com/40633135): The regex doesn't match IBANs for Saint Lucia (LC),
// Kazakhstan (KZ) and Romania (RO). Consider replace the regex with something
// like "(?:IT|SM)\d{2}[A-Z]\d{22}|CY\d{2}[A-Z]\d{23}...". For reference:
//    - https://www.swift.com/resource/iban-registry-pdf
inline constexpr char16_t kInternationalBankAccountNumberValueRe[] =
    u"^[a-zA-Z]{2}[0-9]{2}[a-zA-Z0-9]{4}[0-9]{7}([a-zA-Z0-9]?){0,16}$";

// Matches all 3 and 4 digit numbers.
inline constexpr char16_t kCreditCardCVCPattern[] = u"^\\d{3,4}$";

// Matches numbers in the range [2010-2099].
inline constexpr char16_t kCreditCard4DigitExpYearPattern[] =
    u"^[2][0][1-9][0-9]$";

/////////////////////////////////////////////////////////////////////////////
// form_structure.cc
/////////////////////////////////////////////////////////////////////////////

// Match the path values for form actions that look like generic search:
//  e.g. /search
//       /search/
//       /search/products...
//       /products/search/
//       /blah/search_all.jsp
inline constexpr char16_t kUrlSearchActionRe[] =
    u"/search(/|((\\w*\\.\\w+)?$))";

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_REGEX_CONSTANTS_H_
