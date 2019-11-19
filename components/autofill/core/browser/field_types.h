// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_TYPES_H_

#include <map>
#include <set>

#include "base/strings/string16.h"

namespace autofill {

// NOTE: This list MUST not be modified except to keep it synchronized with the
// Autofill server's version.  The server aggregates and stores these types over
// several versions, so we must remain fully compatible with the Autofill
// server, which is itself backward-compatible.  The list must be kept up to
// date with the Autofill server list.
//
// The list of all field types natively understood by the Autofill server.  A
// subset of these types is used to store Autofill data in the user's profile.
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
  PHONE_HOME_NUMBER = 10,
  PHONE_HOME_CITY_CODE = 11,
  PHONE_HOME_COUNTRY_CODE = 12,
  PHONE_HOME_CITY_AND_NUMBER = 13,
  PHONE_HOME_WHOLE_NUMBER = 14,

  // Work phone numbers (values [15,19]) are deprecated.

  // Fax numbers (values [20,24]) are deprecated in Chrome, but still supported
  // by the server.
  PHONE_FAX_NUMBER = 20,
  PHONE_FAX_CITY_CODE = 21,
  PHONE_FAX_COUNTRY_CODE = 22,
  PHONE_FAX_CITY_AND_NUMBER = 23,
  PHONE_FAX_WHOLE_NUMBER = 24,

  // Cell phone numbers (values [25, 29]) are deprecated.

  ADDRESS_HOME_LINE1 = 30,
  ADDRESS_HOME_LINE2 = 31,
  ADDRESS_HOME_APT_NUM = 32,
  ADDRESS_HOME_CITY = 33,
  ADDRESS_HOME_STATE = 34,
  ADDRESS_HOME_ZIP = 35,
  ADDRESS_HOME_COUNTRY = 36,
  ADDRESS_BILLING_LINE1 = 37,
  ADDRESS_BILLING_LINE2 = 38,
  ADDRESS_BILLING_APT_NUM = 39,
  ADDRESS_BILLING_CITY = 40,
  ADDRESS_BILLING_STATE = 41,
  ADDRESS_BILLING_ZIP = 42,
  ADDRESS_BILLING_COUNTRY = 43,

  // ADDRESS_SHIPPING values [44,50] are deprecated.

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

  PHONE_BILLING_NUMBER = 62,
  PHONE_BILLING_CITY_CODE = 63,
  PHONE_BILLING_COUNTRY_CODE = 64,
  PHONE_BILLING_CITY_AND_NUMBER = 65,
  PHONE_BILLING_WHOLE_NUMBER = 66,

  NAME_BILLING_FIRST = 67,
  NAME_BILLING_MIDDLE = 68,
  NAME_BILLING_LAST = 69,
  NAME_BILLING_MIDDLE_INITIAL = 70,
  NAME_BILLING_FULL = 71,
  NAME_BILLING_SUFFIX = 72,

  // Field types for options generally found in merchant buyflows. Given that
  // these are likely to be filled out differently on a case by case basis,
  // they are here primarily for use by Autocheckout.
  MERCHANT_EMAIL_SIGNUP = 73,
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
  ADDRESS_BILLING_STREET_ADDRESS = 78,

  // A sorting code is similar to a postal code. However, whereas a postal code
  // normally refers to a single geographical location, a sorting code often
  // does not. Instead, a sorting code is assigned to an organization, which
  // might be geographically distributed. The most prominent example of a
  // sorting code system is CEDEX in France.
  ADDRESS_HOME_SORTING_CODE = 79,
  ADDRESS_BILLING_SORTING_CODE = 80,

  // A dependent locality is a subunit of a locality, where a "locality" is
  // roughly equivalent to a city. Examples of dependent localities include
  // inner-city districts and suburbs.
  ADDRESS_HOME_DEPENDENT_LOCALITY = 81,
  ADDRESS_BILLING_DEPENDENT_LOCALITY = 82,

  // The third line of the street address.
  ADDRESS_HOME_LINE3 = 83,
  ADDRESS_BILLING_LINE3 = 84,

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
  // Currently not used by Chrome.
  ADDRESS_HOME_STREET = 103,

  // House number of an address, may be alphanumeric.
  // Currently not used by Chrome.
  ADDRESS_HOME_HOUSE_NUMBER = 104,

  // Floor within in a building, may be alphanumeric.
  // Currently not used by Chrome.
  ADDRESS_HOME_FLOOR = 105,

  // A catch-all for other type of subunits (only used until something more
  // precise is defined).
  // Currently not used by Chrome.
  ADDRESS_HOME_OTHER_SUBUNIT = 106,

  // No new types can be added without a corresponding change to the Autofill
  // server.
  MAX_VALID_FIELD_TYPE = 107,
};

// The list of all HTML autocomplete field type hints supported by Chrome.
// See [ http://is.gd/whatwg_autocomplete ] for the full list of specced hints.
enum HtmlFieldType {
  // Default type.
  HTML_TYPE_UNSPECIFIED,

  // Name types.
  HTML_TYPE_NAME,
  HTML_TYPE_GIVEN_NAME,
  HTML_TYPE_ADDITIONAL_NAME,
  HTML_TYPE_FAMILY_NAME,

  // Business types.
  HTML_TYPE_ORGANIZATION,

  // Address types.
  HTML_TYPE_STREET_ADDRESS,
  HTML_TYPE_ADDRESS_LINE1,
  HTML_TYPE_ADDRESS_LINE2,
  HTML_TYPE_ADDRESS_LINE3,
  HTML_TYPE_ADDRESS_LEVEL1,  // For U.S. addresses, corresponds to the state.
  HTML_TYPE_ADDRESS_LEVEL2,  // For U.S. addresses, corresponds to the city.
  HTML_TYPE_ADDRESS_LEVEL3,  // An area that is more specific than LEVEL2.
  HTML_TYPE_COUNTRY_CODE,    // The ISO 3166-1-alpha-2 country code.
  HTML_TYPE_COUNTRY_NAME,    // The localized country name.
  HTML_TYPE_POSTAL_CODE,
  HTML_TYPE_FULL_ADDRESS,  // The complete address, formatted for display.

  // Credit card types.
  HTML_TYPE_CREDIT_CARD_NAME_FULL,
  HTML_TYPE_CREDIT_CARD_NAME_FIRST,
  HTML_TYPE_CREDIT_CARD_NAME_LAST,
  HTML_TYPE_CREDIT_CARD_NUMBER,
  HTML_TYPE_CREDIT_CARD_EXP,
  HTML_TYPE_CREDIT_CARD_EXP_MONTH,
  HTML_TYPE_CREDIT_CARD_EXP_YEAR,
  HTML_TYPE_CREDIT_CARD_VERIFICATION_CODE,
  HTML_TYPE_CREDIT_CARD_TYPE,

  // Phone number types.
  HTML_TYPE_TEL,
  HTML_TYPE_TEL_COUNTRY_CODE,
  HTML_TYPE_TEL_NATIONAL,
  HTML_TYPE_TEL_AREA_CODE,
  HTML_TYPE_TEL_LOCAL,
  HTML_TYPE_TEL_LOCAL_PREFIX,
  HTML_TYPE_TEL_LOCAL_SUFFIX,
  HTML_TYPE_TEL_EXTENSION,

  // Email.
  HTML_TYPE_EMAIL,

  // Transaction details.
  HTML_TYPE_TRANSACTION_AMOUNT,
  HTML_TYPE_TRANSACTION_CURRENCY,

  // Variants of type hints specified in the HTML specification that are
  // inferred based on a field's 'maxlength' attribute.
  // TODO(isherman): Remove these types, in favor of understanding maxlength
  // when filling fields.  See also: AutofillField::phone_part_.
  HTML_TYPE_ADDITIONAL_NAME_INITIAL,
  HTML_TYPE_CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
  HTML_TYPE_CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
  HTML_TYPE_CREDIT_CARD_EXP_2_DIGIT_YEAR,
  HTML_TYPE_CREDIT_CARD_EXP_4_DIGIT_YEAR,

  // Universal Payment Interface - Virtual Payment Address.
  HTML_TYPE_UPI_VPA,

  // Non-standard autocomplete types.
  HTML_TYPE_UNRECOGNIZED,
};

// The list of all HTML autocomplete field mode hints supported by Chrome.
// See [ http://is.gd/whatwg_autocomplete ] for the full list of specced hints.
enum HtmlFieldMode {
  HTML_MODE_NONE,
  HTML_MODE_BILLING,
  HTML_MODE_SHIPPING,
};

enum FieldTypeGroup {
  NO_GROUP,
  NAME,
  NAME_BILLING,
  EMAIL,
  COMPANY,
  ADDRESS_HOME,
  ADDRESS_BILLING,
  PHONE_HOME,
  PHONE_BILLING,
  CREDIT_CARD,
  PASSWORD_FIELD,
  TRANSACTION,
  USERNAME_FIELD,
  UNFILLABLE,
};

typedef std::set<ServerFieldType> ServerFieldTypeSet;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_TYPES_H_
