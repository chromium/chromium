// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_TYPES_H_

#include <type_traits>

#include "base/strings/string_piece_forward.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/html_field_types.h"

namespace autofill {

// NOTE: This list MUST not be modified except to keep it synchronized with the
// Autofill server's version. The server aggregates and stores these types over
// several versions, so we must remain fully compatible with the Autofill
// server, which is itself backward-compatible. The list must be kept up to
// date with the Autofill server list.
//
// NOTE: When deprecating field types, also update IsValidServerFieldType().
//
// This enum represents the list of all field types natively understood by the
// Autofill server. A subset of these types is used to store Autofill data in
// the user's profile.
//
// # Phone numbers
//
// Here are some examples for how to understand the field types for phone
// numbers:
// - US phone number: (650) 234-5678 - US has the country code +1.
// - German phone number: 089 123456 - Germany has the country code +49.
//
// In the following examples whitespaces are only added for readability
// purposes.
//
// PHONE_HOME_COUNTRY_CODE
//   - US: 1
//   - DE: 49
//
// PHONE_HOME_CITY_CODE: City code without a trunk prefix. Used in combination
//   with a PHONE_HOME_COUNTRY_CODE.
//   - US: 650
//   - DE: 89
// PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX: Like PHONE_HOME_CITY_CODE
//   with a trunk prefix, if applicable in the number's region. Used when no
//   PHONE_HOME_COUNTRY_CODE field is present.
//   - US: 650
//   - DE: 089
//
// PHONE_HOME_NUMBER: Local number without country code and city/area code
//   - US: 234 5678
//   - DE: 123456
//
// PHONE_HOME_NUMBER_PREFIX:
// PHONE_HOME_NUMBER_SUFFIX:
//   PHONE_HOME_NUMBER = PHONE_HOME_NUMBER_PREFIX + PHONE_HOME_NUMBER_SUFFIX.
//   For the US numbers (650) 234-5678 the types correspond to 234 and 5678.
//   The 650 is a PHONE_HOME_CITY_CODE or
//   PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX.
//   The concept of prefix and suffix is not well defined in the standard
//   and based on observations from countries which use prefixes and suffixes we
//   chose that suffixes cover the last 4 digits of the home number and the
//   prefix the rest.
//
// PHONE_HOME_CITY_AND_NUMBER: city and local number with a local trunk prefix
//   where applicable. This is how one would dial the number from within its
//   country.
//   - US: 650 234 5678
//   - DE: 089 123456
// PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX: Like
//   PHONE_HOME_CITY_AND_NUMBER, but never includes a trunk prefix. Used in
//   combination with a PHONE_HOME_COUNTRY_CODE field.
//   - US: 650 234 5678
//   - DE: 89 123456
//
// PHONE_HOME_WHOLE_NUMBER: The phone number in the internal storage
// representation, attempting to preserve the formatting the user provided. As
// such, this number can be in national and international representation.
// If the user made no attempt at formatting the number (it consists only of
// characters of the set [+0123456789], no whitespaces, no parentheses, no
// hyphens, no slashes, etc), we will make an attempt to format the number in a
// proper way. If AutofillInferCountryCallingCode is enabled, we will infer the
// country code and also store that in the formatted number.
// If a website contains <input autocomplete="tel"> this is what we fill. I.e.,
// the phone number representation the user tried to give us.
// With AutofillInferCountryCallingCode, the GetInfo() representation always
// contains a country code. So for filling purposes, PHONE_HOME_WHOLE_NUMBER is
// in international format.
// If we reformat the number ourselves, the GetRawInfo() contains the inferred
// country code. If we don't reformat the number, the GetRawInfo()
// representation remains without one. In all countries but the US and Canada,
// formatting will put a + infront of the country code.
// TODO(crbug.com/1311937) Clean this up once AutofillInferCountryCallingCode
// is launched.
//
// PHONE_HOME_EXTENSION: Extensions are detected, but not filled. This would
//   be the part that comes after a PHONE_HOME_WHOLE_NUMBER or
//   PHONE_HOME_CITY_AND_NUMBER
//
// The following would be reasonable representations of phone numbers:
// - International formats:
//   - WHOLE_NUMBER
//   - COUNTRY_CODE, CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX
//   - COUNTRY_CODE, CITY_CODE, HOME_NUMBER
//   - COUNTRY_CODE, CITY_CODE, NUMBER_PREFIX, NUMBER_SUFFIX
// - National formats:
//   - WHOLE_NUMBER
//   - CITY_AND_NUMBER
//   - CITY_CODE_WITH_TRUNK_PREFIX, PHONE_HOME_NUMBER
//   - CITY_CODE_WITH_TRUNK_PREFIX, NUMBER_PREFIX, NUMBER_SUFFIX
//
// There are a few subtleties to be aware of:
//
// GetRawInfo() can only be used to access the PHONE_HOME_WHOLE_NUMBER.
// It returns a formatted number. If the number was not preformatted by the user
// (i.e. containing formatting characters outside of [+0123456789], we format
// it ourselves.
//
// GetInfo() returns an unformatted number (digits only). It is used for
// filling!
//
enum ServerFieldType {
  // Server indication that it has no data for the requested field.
  NO_SERVER_DATA = 0,
  // Client indication that the text entered did not match anything in the
  // personal data.
  UNKNOWN_TYPE = 1,
  // The "empty" type indicates that the user hasn't entered anything
  // in this field.
  EMPTY_TYPE = 2,
  // Personal Information categorization types.
  NAME_FIRST = 3,
  NAME_MIDDLE = 4,
  NAME_LAST = 5,
  NAME_MIDDLE_INITIAL = 6,
  NAME_FULL = 7,
  NAME_SUFFIX = 8,
  EMAIL_ADDRESS = 9,
  // Local number without country code and city/area code.
  PHONE_HOME_NUMBER = 10,
  // Never includes a trunk prefix. Used in combination with a
  // PHONE_HOME_COUNTRY_CODE field.
  PHONE_HOME_CITY_CODE = 11,
  PHONE_HOME_COUNTRY_CODE = 12,
  // A number in national format and with a trunk prefix, if applicable in the
  // number's region. Used when no PHONE_HOME_COUNTRY_CODE field is present.
  PHONE_HOME_CITY_AND_NUMBER = 13,
  PHONE_HOME_WHOLE_NUMBER = 14,

  // Work phone numbers (values [15,19]) are deprecated.
  // Fax numbers (values [20,24]) are deprecated.
  // Cell phone numbers (values [25, 29]) are deprecated.

  ADDRESS_HOME_LINE1 = 30,
  ADDRESS_HOME_LINE2 = 31,
  ADDRESS_HOME_APT_NUM = 32,
  ADDRESS_HOME_CITY = 33,
  ADDRESS_HOME_STATE = 34,
  ADDRESS_HOME_ZIP = 35,
  ADDRESS_HOME_COUNTRY = 36,

  // ADDRESS_BILLING values [37, 43] are deprecated.
  // ADDRESS_SHIPPING values [44, 50] are deprecated.

  CREDIT_CARD_NAME_FULL = 51,
  CREDIT_CARD_NUMBER = 52,
  CREDIT_CARD_EXP_MONTH = 53,
  CREDIT_CARD_EXP_2_DIGIT_YEAR = 54,
  CREDIT_CARD_EXP_4_DIGIT_YEAR = 55,
  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR = 56,
  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR = 57,
  CREDIT_CARD_TYPE = 58,
  CREDIT_CARD_VERIFICATION_CODE = 59,

  COMPANY_NAME = 60,

  // Generic type whose default value is known.
  FIELD_WITH_DEFAULT_VALUE = 61,

  // PHONE_BILLING values [62, 66] are deprecated.
  // NAME_BILLING values [67, 72] are deprecated.

  // Field types for options generally found in merchant buyflows. Given that
  // these are likely to be filled out differently on a case by case basis,
  // they are here primarily for use by Autocheckout.
  MERCHANT_EMAIL_SIGNUP = 73,
  // A promo/gift/coupon code, usually entered during checkout on a commerce web
  // site to reduce the cost of a purchase.
  MERCHANT_PROMO_CODE = 74,

  // Field types for the password fields. PASSWORD is the default type for all
  // password fields. ACCOUNT_CREATION_PASSWORD is the first password field in
  // an account creation form and will trigger password generation.
  PASSWORD = 75,
  ACCOUNT_CREATION_PASSWORD = 76,

  // Includes all of the lines of a street address, including newlines, e.g.
  //   123 Main Street,
  //   Apt. #42
  ADDRESS_HOME_STREET_ADDRESS = 77,
  // ADDRESS_BILLING_STREET_ADDRESS 78 is deprecated.

  // A sorting code is similar to a postal code. However, whereas a postal code
  // normally refers to a single geographical location, a sorting code often
  // does not. Instead, a sorting code is assigned to an organization, which
  // might be geographically distributed. The most prominent example of a
  // sorting code system is CEDEX in France.
  ADDRESS_HOME_SORTING_CODE = 79,
  // ADDRESS_BILLING_SORTING_CODE 80 is deprecated.

  // A dependent locality is a subunit of a locality, where a "locality" is
  // roughly equivalent to a city. Examples of dependent localities include
  // inner-city districts and suburbs.
  ADDRESS_HOME_DEPENDENT_LOCALITY = 81,
  // ADDRESS_BILLING_DEPENDENT_LOCALITY 82 is deprecated.

  // The third line of the street address.
  ADDRESS_HOME_LINE3 = 83,
  // ADDRESS_BILLING_LINE3 84 is deprecated.

  // Inverse of ACCOUNT_CREATION_PASSWORD. Sent when there is data that
  // a previous upload of ACCOUNT_CREATION_PASSWORD was incorrect.
  NOT_ACCOUNT_CREATION_PASSWORD = 85,

  // Field types for username fields in password forms.
  USERNAME = 86,
  USERNAME_AND_EMAIL_ADDRESS = 87,

  // Field types related to new password fields on change password forms.
  NEW_PASSWORD = 88,
  PROBABLY_NEW_PASSWORD = 89,
  NOT_NEW_PASSWORD = 90,

  // Additional field types for credit card fields.
  CREDIT_CARD_NAME_FIRST = 91,
  CREDIT_CARD_NAME_LAST = 92,

  // Extensions are detected, but not filled.
  PHONE_HOME_EXTENSION = 93,

  // PROBABLY_ACCOUNT_CREATION_PASSWORD value 94 is deprecated.

  // The confirmation password field in account creation or change password
  // forms.
  CONFIRMATION_PASSWORD = 95,

  // The data entered by the user matches multiple pieces of autofill data,
  // none of which were predicted by autofill. This value is used for metrics
  // only, it is not a predicted nor uploaded type.
  AMBIGUOUS_TYPE = 96,

  // Search term fields are detected, but not filled.
  SEARCH_TERM = 97,

  // Price fields are detected, but not filled.
  PRICE = 98,

  // Password-type fields which are not actual passwords.
  NOT_PASSWORD = 99,

  // Username field when there is no corresponding password field. It might be
  // because of:
  // 1. Username first flow: a user has to type username first on one page and
  // then password on another page
  // 2. Username and password fields are in different <form>s.
  SINGLE_USERNAME = 100,

  // Text-type fields which are not usernames.
  NOT_USERNAME = 101,

  // UPI/VPA is a payment method, which is stored and filled. See
  // https://en.wikipedia.org/wiki/Unified_Payments_Interface
  UPI_VPA = 102,

  // Just the street name of an address, no house number.
  ADDRESS_HOME_STREET_NAME = 103,

  // House number of an address, may be alphanumeric.
  ADDRESS_HOME_HOUSE_NUMBER = 104,

  // Contains the floor, the staircase the apartment number within a building.
  ADDRESS_HOME_SUBPREMISE = 105,

  // A catch-all for other type of subunits (only used until something more
  // precise is defined).
  // Currently not used by Chrome.
  ADDRESS_HOME_OTHER_SUBUNIT = 106,

  // Types to represent the structure of a Hispanic/Latinx last name.
  NAME_LAST_FIRST = 107,
  NAME_LAST_CONJUNCTION = 108,
  NAME_LAST_SECOND = 109,

  // Type to catch name additions like "Mr.", "Ms." or "Dr.".
  NAME_HONORIFIC_PREFIX = 110,

  // Type that corresponds to the name of a place or a building below the
  // granularity of a street.
  ADDRESS_HOME_PREMISE_NAME = 111,

  // Type that describes a crossing street as it is used in some countries to
  // describe a location.
  ADDRESS_HOME_DEPENDENT_STREET_NAME = 112,

  // Compound type to join the street and dependent street names.
  ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME = 113,

  // The complete formatted address as it would be written on an envelope or in
  // a clear-text field without the name.
  ADDRESS_HOME_ADDRESS = 114,

  // The complete formatted address including the name.
  ADDRESS_HOME_ADDRESS_WITH_NAME = 115,

  // The floor number within a building.
  ADDRESS_HOME_FLOOR = 116,

  // The full name including the honorific prefix.
  NAME_FULL_WITH_HONORIFIC_PREFIX = 117,

  // Types to represent a birthdate.
  BIRTHDATE_DAY = 118,
  BIRTHDATE_MONTH = 119,
  BIRTHDATE_4_DIGIT_YEAR = 120,

  // Types for better trunk prefix support for phone numbers.
  // Like PHONE_HOME_CITY_CODE, but with a trunk prefix, if applicable in the
  // number's region. Used when no PHONE_HOME_COUNTRY_CODE field is present.
  PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX = 121,
  // Like PHONE_HOME_CITY_AND_NUMBER, but never includes a trunk prefix. Used in
  // combination with a PHONE_HOME_COUNTRY_CODE field.
  PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX = 122,

  // PHONE_HOME_NUMBER = PHONE_HOME_NUMBER_PREFIX + PHONE_HOME_NUMBER_SUFFIX.
  // For the US numbers (650) 234-5678 the types correspond to 234 and 5678.
  PHONE_HOME_NUMBER_PREFIX = 123,
  PHONE_HOME_NUMBER_SUFFIX = 124,

  // International Bank Account Number (IBAN) details are usually entered on
  // banking and merchant websites used to make international transactions.
  // See https://en.wikipedia.org/wiki/International_Bank_Account_Number.
  IBAN_VALUE = 125,

  // Standalone card verification code (CVC).
  CREDIT_CARD_STANDALONE_VERIFICATION_CODE = 126,

  // Reserved for a server-side-only use: 127

  // Type of a field that asks for a numeric quantity. Not fillable by Autofill.
  // The purpose is to ignore false positive server classification for numeric
  // types that a prone to false-positive votes.
  NUMERIC_QUANTITY = 128,

  // No new types can be added without a corresponding change to the Autofill
  // server.
  // Please update enum `AutofillServerFieldType` in
  // `tools/metrics/histogram/enums.xml` for metrics tracking.
  MAX_VALID_FIELD_TYPE = 129,
};

enum class FieldTypeGroup {
  kNoGroup,
  kName,
  kNameBilling,
  kEmail,
  kCompany,
  kAddressHome,
  kAddressBilling,
  kPhoneHome,
  kPhoneBilling,
  kCreditCard,
  kPasswordField,
  kTransaction,
  kUsernameField,
  kUnfillable,
  kBirthdateField,
  kMaxValue = kBirthdateField,
};

using ServerFieldTypeSet = DenseSet<ServerFieldType, MAX_VALID_FIELD_TYPE>;

std::ostream& operator<<(std::ostream& o, ServerFieldTypeSet field_type_set);

// Returns |raw_value| if it corresponds to a non-deprecated enumeration
// constant of ServerFieldType other than MAX_VALID_FIELD_TYPE. Otherwise,
// returns |fallback_value|.
ServerFieldType ToSafeServerFieldType(
    std::underlying_type_t<ServerFieldType> raw_value,
    ServerFieldType fallback_value);

// Returns whether the field can be filled with data.
bool IsFillableFieldType(ServerFieldType field_type);

// Returns a StringPiece describing |type|. As the StringPiece points to a
// static string, you don't need to worry about memory deallocation.
base::StringPiece FieldTypeToStringPiece(ServerFieldType type);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_TYPES_H_
