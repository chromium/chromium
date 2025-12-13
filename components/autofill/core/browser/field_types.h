// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_TYPES_H_

#include <type_traits>

#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/html_field_types.h"

namespace autofill {

// NOTE: This list MUST not be modified except to keep it synchronized with the
// Autofill server's version. The server aggregates and stores these types over
// several versions, so we must remain fully compatible with the Autofill
// server, which is itself backward-compatible. The list must be kept up to
// date with the Autofill server list.
//
// NOTE: When deprecating field types, also update IsValidFieldType().
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
// proper way. We will infer the country code and also store that in the
// formatted number. If a website contains <input autocomplete="tel"> this is
// what we fill. I.e., the phone number representation the user tried to give
// us. The GetInfo() representation always contains a country code. So for
// filling purposes, PHONE_HOME_WHOLE_NUMBER is in international format. If we
// reformat the number ourselves, the GetRawInfo() contains the inferred country
// code. If we don't reformat the number, the GetRawInfo() representation
// remains without one. In all countries but the US and Canada, formatting will
// put a + in front of the country code.
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
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill
//
// LINT.IfChange
// This enum set must be kept in sync with IDL enum used by JS code.
enum FieldType {
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
  // The raw number (or identifier) of an apartment (e.g. "5") but without a
  // prefix. The value "apt 5" would correspond to an ADDRESS_HOME_APT.
  ADDRESS_HOME_APT_NUM = 32,
  ADDRESS_HOME_CITY = 33,
  ADDRESS_HOME_STATE = 34,
  ADDRESS_HOME_ZIP = 35,
  // TODO(crbug.com/40264633): Autofill stores country codes. When
  // ADDRESS_HOME_COUNTRY is accessed through `AutofillProfile::GetRawInfo()`, a
  // country code is returned. When retrieved using
  // `AutofillProfile::GetInfo()`, the country name is returned.
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

  // FIELD_WITH_DEFAULT_VALUE value 61 is deprecated.
  // PHONE_BILLING values [62, 66] are deprecated.
  // NAME_BILLING values [67, 72] are deprecated.

  // Field types for options generally found in merchant buyflows. Given that
  // these are likely to be filled out differently on a case by case basis,
  // they are here primarily for use by AutoCheckout.
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
  // UPI_VPA value 102 is deprecated.

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

  // ADDRESS_HOME_PREMISE_NAME value 111 is deprecated.

  // ADDRESS_HOME_DEPENDENT_STREET_NAME value 112 is deprecated.

  // Compound type to join the street and dependent street names.
  // ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME  value 113 is deprecated.

  // The complete formatted address as it would be written on an envelope or in
  // a clear-text field without the name.
  ADDRESS_HOME_ADDRESS = 114,

  // The complete formatted address including the name.
  ADDRESS_HOME_ADDRESS_WITH_NAME = 115,

  // The floor number within a building.
  ADDRESS_HOME_FLOOR = 116,

  // NAME_FULL_WITH_HONORIFIC_PREFIX value 117 is deprecated.

  // Birthdates 118, 119 and 120 are deprecated.

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

  // One-time code used for verifying user identity.
  ONE_TIME_CODE = 129,

  // Type for additional delivery instructions to find the address.
  DELIVERY_INSTRUCTIONS = 133,

  // Additional information for describing the location within a building or
  // gated community. Often called "extra information", "additional
  // information", "address extension", etc.
  ADDRESS_HOME_OVERFLOW = 135,

  // A well-known object or feature of the landscape that can easily be
  // recognized to understand where the building is situated.
  ADDRESS_HOME_LANDMARK = 136,

  // Combination of types ADDRESS_HOME_OVERFLOW and ADDRESS_HOME_LANDMARK.
  ADDRESS_HOME_OVERFLOW_AND_LANDMARK = 140,

  // Administrative area level 2. A sub-division of a state, e.g. a Municipio in
  // Brazil or Mexico.
  ADDRESS_HOME_ADMIN_LEVEL2 = 141,

  // Street name and house number in structured address forms. Should NOT be
  // used for US.
  ADDRESS_HOME_STREET_LOCATION = 142,

  // The type indicates that the address is at the intersection between two
  // streets. This is a common way of writing addresses in Mexico.
  ADDRESS_HOME_BETWEEN_STREETS = 143,

  // Combination of types ADDRESS_HOME_BETWEEN_STREETS or ADDRESS_HOME_LANDMARK.
  ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK = 144,

  // Combination of types ADDRESS_HOME_STREET_LOCATION and
  // ADDRESS_HOME_DEPENDENT_LOCALITY.
  ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY = 145,

  // Combination of types ADDRESS_HOME_STREET_LOCATION and
  // ADDRESS_HOME_LANDMARK.
  // One of the synthesized types in the address model in India.
  ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK = 146,

  // Combination of types ADDRESS_HOME_DEPENDENT_LOCALITY and
  // ADDRESS_HOME_LANDMARK.
  // One of the synthesized types in the address model in India.
  ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK = 150,

  // The meaning of the field is the same as ADDRESS_HOME_BETWEEN_STREETS. The
  // field type should be used for "Entre calle 1" in MX forms which also
  // contain the "Entre calle 2" field.
  ADDRESS_HOME_BETWEEN_STREETS_1 = 151,

  // The meaning of the field is the same as ADDRESS_HOME_BETWEEN_STREETS. The
  // field type should be used for "Entre calle 2" in MX forms which also
  // contain the "Entre calle 1" field.
  ADDRESS_HOME_BETWEEN_STREETS_2 = 152,

  // House number and apartment.
  ADDRESS_HOME_HOUSE_NUMBER_AND_APT = 153,

  // Username field in a password-less forgot password form.
  SINGLE_USERNAME_FORGOT_PASSWORD = 154,

  // Autofill fallback type for username fields which accept also email or
  // phone number.
  // EMAIL_OR_PHONE_NUMBER = 155 is server-side only.

  // All the information related to the apartment. Normally a combination of the
  // apartment type (ADDRESS_HOME_APT_TYPE) and number (ADDRESS_HOME_APT_NUM).
  // E.g. "Apt 5".
  // ADDRESS_HOME_APT and ADDRESS_HOME_APT_TYPE are intended to remain
  // experimental types (i.e. we don't classify fields with this type) because
  // we don't expect that fields ask for "Apt" or "Apt 5" as entries for
  // example. There is a risk that "Apt 5" votes might turn ADDRESS_HOME_LINE2
  // into ADDRESS_HOME_APT entries. We'd need to be very intentional with such a
  // change as it affects the US for example.
  ADDRESS_HOME_APT = 156,

  // Information describing the type of apartment (e.g. Apt, Apartamento, Sala,
  // Departamento).
  ADDRESS_HOME_APT_TYPE = 157,

  // Loyalty program card or membeship ID.
  LOYALTY_MEMBERSHIP_ID = 158,

  // Reserved for a server-side-only use: 159

  // Similar to `SINGLE_USERNAME`, but for the case when there are additional
  // fields between single username and password forms.
  // Will be used to rollout new predictions based on new votes of Username
  // First Flow with intermediate values.
  // TODO(crbug.com/294195764): Deprecate after fully rolling out new
  // predictions.
  SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES = 160,

  // SERVER_RESPONSE_PENDING is not exposed as an enum value to prevent
  // confusion. It is never sent by the server nor sent for voting. The purpose
  // is merely to have a well defined value in the debug attributes if
  // chrome://flags/#show-autofill-type-predictions is enabled. This is not
  // the same as NO_SERVER_DATA, which indicates that the server has no
  // classification for the field.
  // SERVER_RESPONSE_PENDING = 161;

  // IMPROVED_PREDICTION = 162 is deprecated

  // Types to represent alternative names (e.g. phonetic name in Japanese).
  ALTERNATIVE_FULL_NAME = 163,
  ALTERNATIVE_GIVEN_NAME = 164,
  ALTERNATIVE_FAMILY_NAME = 165,

  // Prefix of the last name, e.g. "van" in the Netherlands.
  // This is the first child of NAME_LAST.
  NAME_LAST_PREFIX = 166,

  // Type to represent the core part of the last name.
  // More technically it contains the last name without the prefix.
  // NAME_LAST_CORE: NAME_LAST_FIRST + NAME_LAST_CONJUNCTION + NAME_LAST_SECOND.
  // Don't use this type unless there is NAME_LAST_PREFIX present in the form.
  // E.g. "Gogh" in "Vincent van Gogh".
  NAME_LAST_CORE = 167,

  // Types corresponding to the "Passport" entity from
  // components/autofill/core/browser/data_model/autofill_ai/entity_schema.json.
  // *TAG field types are merely placeholder tagging that the type belongs to
  // the passport entity, but that the existing Autofill classification or logic
  // should be used.
  // PASSPORT_NAME_TAG = 168 is deprecated
  PASSPORT_NUMBER = 169,
  PASSPORT_ISSUING_COUNTRY = 170,
  PASSPORT_EXPIRATION_DATE = 171,
  PASSPORT_ISSUE_DATE = 172,

  // Types corresponding to the "Loyalty card" entity from
  // components/autofill/core/browser/data_model/autofill_ai/entity_schema.json.
  LOYALTY_MEMBERSHIP_PROGRAM = 173,
  LOYALTY_MEMBERSHIP_PROVIDER = 174,
  // The member ID is represented by LOYALTY_MEMBERSHIP_ID.

  // Types corresponding to the "Car" entity from
  // components/autofill/core/browser/data_model/autofill_ai/entity_schema.json.
  // VEHICLE_OWNER_TAG = 175 is deprecated
  VEHICLE_LICENSE_PLATE = 176,
  VEHICLE_VIN = 177,
  VEHICLE_MAKE = 178,
  VEHICLE_MODEL = 179,

  // Types corresponding to the "Drivers license" entity from
  // components/autofill/core/browser/data_model/autofill_ai/entity_schema.json.
  // DRIVERS_LICENSE_NAME_TAG = 180 is deprecated
  DRIVERS_LICENSE_REGION = 181,
  DRIVERS_LICENSE_NUMBER = 182,
  DRIVERS_LICENSE_EXPIRATION_DATE = 183,
  DRIVERS_LICENSE_ISSUE_DATE = 184,

  VEHICLE_YEAR = 185,
  VEHICLE_PLATE_STATE = 186,

  // Types 187 and 188 are not used yet on the client, but will likely be added
  // in the future.

  // For fields that can contain either email or loyalty membership ID. This
  // type is neither voted for by the client nor emitted by the server. The
  // client will vote for either EMAIL_ADDRESS or LOYALTY_MEMBERSHIP_ID and the
  // server will emit both types in the response. The joined type is built by
  // the client.
  EMAIL_OR_LOYALTY_MEMBERSHIP_ID = 189,

  // Types corresponding to the "National Id Card" entity from
  // components/autofill/core/browser/data_model/autofill_ai/entity_schema.json.
  NATIONAL_ID_CARD_NUMBER = 190,
  NATIONAL_ID_CARD_EXPIRATION_DATE = 191,
  NATIONAL_ID_CARD_ISSUE_DATE = 192,
  NATIONAL_ID_CARD_ISSUING_COUNTRY = 193,

  // Types corresponding to the "Known traveler" entity from
  // components/autofill/core/browser/data_model/autofill_ai/entity_schema.json.
  KNOWN_TRAVELER_NUMBER = 194,
  KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE = 203,

  // Types corresponding to the "Redress number" entity from
  // components/autofill/core/browser/data_model/autofill_ai/entity_schema.json.
  REDRESS_NUMBER = 195,

  // Types 196 and 197 are not used on the client yet, but will likely be added
  // in the future.

  // ADDRESS_HOME_ZIP = ADDRESS_HOME_ZIP_PREFIX + separator +
  // ADDRESS_HOME_ZIP_SUFFIX.
  // For the US zip code 94043-4100 the types correspond to 94043 and 4100.
  ADDRESS_HOME_ZIP_PREFIX = 201,
  ADDRESS_HOME_ZIP_SUFFIX = 202,

  // Types corresponding to the "Flight reservation" entity from
  // components/autofill/core/browser/data_model/autofill_ai/entity_schema.json.
  FLIGHT_RESERVATION_FLIGHT_NUMBER = 198,
  FLIGHT_RESERVATION_CONFIRMATION_CODE = 199,
  FLIGHT_RESERVATION_TICKET_NUMBER = 200,
  FLIGHT_RESERVATION_DEPARTURE_AIRPORT = 204,
  FLIGHT_RESERVATION_ARRIVAL_AIRPORT = 205,
  FLIGHT_RESERVATION_DEPARTURE_DATE = 206,

  // No new types can be added without a corresponding change to the Autofill
  // server.
  // This enum must be kept in sync with FieldType from
  // * chrome/common/extensions/api/autofill_private.idl
  // * tools/typescript/definitions/autofill_private.d.ts
  // Please update `tools/metrics/histograms/enums.xml` by executing
  // `tools/metrics/histograms/update_autofill_enums.py`.
  // If the newly added type is a storable type of AutofillProfile, update
  // AutofillProfile.StorableTypes in
  // tools/metrics/histograms/metadata/autofill/histograms.xml.
  MAX_VALID_FIELD_TYPE = 207,
};
// LINT.ThenChange(//chrome/common/extensions/api/autofill_private.idl)

enum class FieldTypeGroup {
  kNoGroup,
  kName,
  kEmail,
  kCompany,
  kAddress,
  kPhone,
  kCreditCard,
  kPasswordField,
  kTransaction,
  kUsernameField,
  kUnfillable,
  kIban,
  kStandaloneCvcField,
  kAutofillAi,
  kLoyaltyCard,
  kOneTimePassword,
  kMaxValue = kOneTimePassword,
};

constexpr FieldType ToSafeFieldType(std::underlying_type_t<FieldType> raw_value,
                                    FieldType fallback_value);

constexpr HtmlFieldType ToSafeHtmlFieldType(
    std::underlying_type_t<HtmlFieldType> raw_value,
    HtmlFieldType fallback_value);

template <>
struct DenseSetTraits<FieldType>
    : EnumDenseSetTraits<FieldType, NO_SERVER_DATA, MAX_VALID_FIELD_TYPE> {
  static constexpr bool is_valid(FieldType x) {
    return x == NO_SERVER_DATA ||
           ToSafeFieldType(base::to_underlying(x), NO_SERVER_DATA) !=
               NO_SERVER_DATA;
  }
};

template <>
struct DenseSetTraits<HtmlFieldType>
    : EnumDenseSetTraits<HtmlFieldType,
                         HtmlFieldType(0),
                         HtmlFieldType::kMaxValue> {
  static constexpr bool is_valid(HtmlFieldType x) {
    return x == HtmlFieldType::kUnrecognized ||
           ToSafeHtmlFieldType(base::to_underlying(x),
                               HtmlFieldType::kUnrecognized) !=
               HtmlFieldType::kUnrecognized;
  }
};

using FieldTypeSet = DenseSet<FieldType>;

using FieldTypeGroupSet = DenseSet<FieldTypeGroup>;

using HtmlFieldTypeSet = DenseSet<HtmlFieldType>;

std::ostream& operator<<(std::ostream& o, FieldTypeSet field_type_set);

// Returns whether the field can be filled with data.
bool IsFillableFieldType(FieldType field_type);

// Returns a string view describing `type`.
std::string_view FieldTypeToStringView(FieldType type);

// Returns a string describing `type`.
std::string FieldTypeToString(FieldType type);

// Returns a comma-separated list of string representations of the elements of
// `s`.
std::string FieldTypeSetToString(FieldTypeSet s);

// Inverse FieldTypeToStringView(). Returns UNKNOWN_TYPE for unknown FieldType
// string representations.
FieldType TypeNameToFieldType(std::string_view type_name);

// Returns a string view describing `type`. The devtools UI uses this string to
// give developers feedback about autofill's filling decision. Note that
// different field types can map to the same string representation for
// simplicity of the feedback. Returns an empty string if the type is not
// supported.
std::string_view FieldTypeToDeveloperRepresentationString(FieldType type);

// There's a one-to-many relationship between FieldTypeGroup and
// FieldType as well as HtmlFieldType.
constexpr FieldTypeSet FieldTypesOfGroup(FieldTypeGroup group);
constexpr FieldTypeGroup GroupTypeOfFieldType(FieldType field_type);
FieldTypeGroup GroupTypeOfHtmlFieldType(HtmlFieldType field_type);

// Not all HtmlFieldTypes have a corresponding FieldType.
FieldType HtmlFieldTypeToBestCorrespondingFieldType(HtmlFieldType field_type);

// Returns |raw_value| if it corresponds to a non-deprecated enumeration
// constant of FieldType other than MAX_VALID_FIELD_TYPE. Otherwise, returns
// |fallback_value|.
constexpr FieldType ToSafeFieldType(std::underlying_type_t<FieldType> raw_value,
                                    FieldType fallback_value) {
  auto is_invalid = [](std::underlying_type_t<FieldType> t) {
    return t < NO_SERVER_DATA || t >= MAX_VALID_FIELD_TYPE ||
           // Work phone numbers (values [15,19]) are deprecated.
           (15 <= t && t <= 19) ||
           // Cell phone numbers (values [25,29]) are deprecated.
           (25 <= t && t <= 29) ||
           // Shipping addresses (values [44,50]) are deprecated.
           (44 <= t && t <= 50) ||
           // Probably-account creation password (value 94) is deprecated.
           t == 94 ||
           // Billing addresses (values [37,43], 78, 80, 82, 84) are deprecated.
           (37 <= t && t <= 43) || t == 78 || t == 80 || t == 82 || t == 84 ||
           // FIELD_WITH_DEFAULT_VALUE is deprecated.
           t == 61 ||
           // Billing phone numbers (values [62,66]) are deprecated.
           (62 <= t && t <= 66) ||
           // Billing names (values [67,72]) are deprecated.
           (67 <= t && t <= 72) ||
           // Fax numbers (values [20,24]) are deprecated.
           (20 <= t && t <= 24) ||
           // UPI VPA type (value 102) is deprecated.
           t == 102 ||
           // Birthdates (values [118, 120]) are deprecated.
           (118 <= t && t <= 120) ||
           // Reserved for server-side only use.
           (111 <= t && t <= 113) || t == 117 || t == 127 ||
           (130 <= t && t <= 132) || t == 134 || (137 <= t && t <= 139) ||
           (147 <= t && t <= 149) || t == 155 || t == 159 || t == 161 ||
           // Deprecated Autofill AI types.
           t == 162 || t == 168 || t == 175 || t == 180 ||
           // Types for the country for driver's license and vehicle are not
           // used yet, but will likely be added in the future.
           (187 <= t && t <= 188) ||
           // Types for date of birth and gender are not used yet, but will
           // likely be added in the future.
           (196 <= t && t <= 197);
  };
  return is_invalid(raw_value) ? fallback_value
                               : static_cast<FieldType>(raw_value);  // nocheck
}

constexpr HtmlFieldType ToSafeHtmlFieldType(
    std::underlying_type_t<HtmlFieldType> raw_value,
    HtmlFieldType fallback_value) {
  auto is_invalid = [](std::underlying_type_t<HtmlFieldType> t) {
    return t < base::to_underlying(HtmlFieldType::kMinValue) ||
           t > base::to_underlying(HtmlFieldType::kMaxValue) ||
           // Full address is deprecated.
           t == 17 ||
           // UPI is deprecated.
           t == 46;
  };
  return is_invalid(raw_value) ? fallback_value
                               : static_cast<HtmlFieldType>(raw_value);
}

constexpr FieldTypeGroup GroupTypeOfFieldType(FieldType field_type) {
  switch (field_type) {
    case NAME_HONORIFIC_PREFIX:
    case NAME_FIRST:
    case NAME_MIDDLE:
    case NAME_LAST:
    case NAME_LAST_PREFIX:
    case NAME_LAST_CORE:
    case NAME_LAST_FIRST:
    case NAME_LAST_SECOND:
    case NAME_LAST_CONJUNCTION:
    case NAME_MIDDLE_INITIAL:
    case NAME_FULL:
    case NAME_SUFFIX:
    case ALTERNATIVE_FAMILY_NAME:
    case ALTERNATIVE_GIVEN_NAME:
    case ALTERNATIVE_FULL_NAME:
      return FieldTypeGroup::kName;

    case EMAIL_ADDRESS:
    case USERNAME_AND_EMAIL_ADDRESS:
    case EMAIL_OR_LOYALTY_MEMBERSHIP_ID:
      return FieldTypeGroup::kEmail;

    case PHONE_HOME_NUMBER:
    case PHONE_HOME_NUMBER_PREFIX:
    case PHONE_HOME_NUMBER_SUFFIX:
    case PHONE_HOME_CITY_CODE:
    case PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
    case PHONE_HOME_COUNTRY_CODE:
    case PHONE_HOME_CITY_AND_NUMBER:
    case PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
    case PHONE_HOME_WHOLE_NUMBER:
    case PHONE_HOME_EXTENSION:
      return FieldTypeGroup::kPhone;

    case ADDRESS_HOME_LINE1:
    case ADDRESS_HOME_LINE2:
    case ADDRESS_HOME_LINE3:
    case ADDRESS_HOME_APT:
    case ADDRESS_HOME_APT_NUM:
    case ADDRESS_HOME_APT_TYPE:
    case ADDRESS_HOME_CITY:
    case ADDRESS_HOME_STATE:
    case ADDRESS_HOME_ZIP:
    case ADDRESS_HOME_ZIP_PREFIX:
    case ADDRESS_HOME_ZIP_SUFFIX:
    case ADDRESS_HOME_COUNTRY:
    case ADDRESS_HOME_STREET_ADDRESS:
    case ADDRESS_HOME_SORTING_CODE:
    case ADDRESS_HOME_DEPENDENT_LOCALITY:
    case ADDRESS_HOME_STREET_NAME:
    case ADDRESS_HOME_HOUSE_NUMBER:
    case ADDRESS_HOME_SUBPREMISE:
    case ADDRESS_HOME_OTHER_SUBUNIT:
    case ADDRESS_HOME_ADDRESS:
    case ADDRESS_HOME_ADDRESS_WITH_NAME:
    case ADDRESS_HOME_FLOOR:
    case ADDRESS_HOME_LANDMARK:
    case ADDRESS_HOME_BETWEEN_STREETS:
    case ADDRESS_HOME_BETWEEN_STREETS_1:
    case ADDRESS_HOME_BETWEEN_STREETS_2:
    case ADDRESS_HOME_ADMIN_LEVEL2:
    case ADDRESS_HOME_STREET_LOCATION:
    case ADDRESS_HOME_OVERFLOW:
    case ADDRESS_HOME_OVERFLOW_AND_LANDMARK:
    case ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK:
    case ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY:
    case ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK:
    case ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK:
    case DELIVERY_INSTRUCTIONS:
    case ADDRESS_HOME_HOUSE_NUMBER_AND_APT:
      return FieldTypeGroup::kAddress;

    case CREDIT_CARD_NAME_FULL:
    case CREDIT_CARD_NAME_FIRST:
    case CREDIT_CARD_NAME_LAST:
    case CREDIT_CARD_NUMBER:
    case CREDIT_CARD_EXP_MONTH:
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
    case CREDIT_CARD_TYPE:
    case CREDIT_CARD_VERIFICATION_CODE:
      return FieldTypeGroup::kCreditCard;

    case CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
      return FieldTypeGroup::kStandaloneCvcField;

    case IBAN_VALUE:
      return FieldTypeGroup::kIban;

    case COMPANY_NAME:
      return FieldTypeGroup::kCompany;

    case PASSPORT_NUMBER:
    case PASSPORT_ISSUING_COUNTRY:
    case PASSPORT_EXPIRATION_DATE:
    case PASSPORT_ISSUE_DATE:
    case VEHICLE_LICENSE_PLATE:
    case VEHICLE_VIN:
    case VEHICLE_MAKE:
    case VEHICLE_MODEL:
    case VEHICLE_YEAR:
    case VEHICLE_PLATE_STATE:
    case DRIVERS_LICENSE_REGION:
    case DRIVERS_LICENSE_NUMBER:
    case DRIVERS_LICENSE_EXPIRATION_DATE:
    case DRIVERS_LICENSE_ISSUE_DATE:
    case NATIONAL_ID_CARD_NUMBER:
    case NATIONAL_ID_CARD_ISSUE_DATE:
    case NATIONAL_ID_CARD_EXPIRATION_DATE:
    case NATIONAL_ID_CARD_ISSUING_COUNTRY:
    case REDRESS_NUMBER:
    case KNOWN_TRAVELER_NUMBER:
    case KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE:
    case FLIGHT_RESERVATION_FLIGHT_NUMBER:
    case FLIGHT_RESERVATION_TICKET_NUMBER:
    case FLIGHT_RESERVATION_CONFIRMATION_CODE:
    case FLIGHT_RESERVATION_DEPARTURE_AIRPORT:
    case FLIGHT_RESERVATION_ARRIVAL_AIRPORT:
    case FLIGHT_RESERVATION_DEPARTURE_DATE:
      return FieldTypeGroup::kAutofillAi;

    case PASSWORD:
    case ACCOUNT_CREATION_PASSWORD:
    case NOT_ACCOUNT_CREATION_PASSWORD:
    case NEW_PASSWORD:
    case PROBABLY_NEW_PASSWORD:
    case NOT_NEW_PASSWORD:
    case CONFIRMATION_PASSWORD:
    case NOT_PASSWORD:
    case SINGLE_USERNAME:
    case NOT_USERNAME:
    case SINGLE_USERNAME_FORGOT_PASSWORD:
    case SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES:
      return FieldTypeGroup::kPasswordField;

    case NO_SERVER_DATA:
    case EMPTY_TYPE:
    case AMBIGUOUS_TYPE:
    case MERCHANT_EMAIL_SIGNUP:
    case MERCHANT_PROMO_CODE:
      return FieldTypeGroup::kNoGroup;

    case ONE_TIME_CODE:
      return FieldTypeGroup::kOneTimePassword;

    case LOYALTY_MEMBERSHIP_ID:
    case LOYALTY_MEMBERSHIP_PROGRAM:
    case LOYALTY_MEMBERSHIP_PROVIDER:
      return FieldTypeGroup::kLoyaltyCard;

    case USERNAME:
      return FieldTypeGroup::kUsernameField;

    case PRICE:
    case SEARCH_TERM:
    case NUMERIC_QUANTITY:
      return FieldTypeGroup::kUnfillable;

    case UNKNOWN_TYPE:
      return FieldTypeGroup::kNoGroup;

    case MAX_VALID_FIELD_TYPE:
      break;
  }
  NOTREACHED();
}

namespace internal {
consteval std::array<FieldTypeSet,
                     base::to_underlying(FieldTypeGroup::kMaxValue) + 1>
FieldTypesByGroupHelper() {
  constexpr auto kMaxValue = base::to_underlying(FieldTypeGroup::kMaxValue);
  std::array<FieldTypeSet, kMaxValue + 1> map{};
  for (FieldType field_type : FieldTypeSet::all()) {
    auto index = base::to_underlying(GroupTypeOfFieldType(field_type));
    map[index].insert(field_type);
  }
  return map;
}
}  // namespace internal

constexpr FieldTypeSet FieldTypesOfGroup(FieldTypeGroup group) {
  return internal::FieldTypesByGroupHelper()[base::to_underlying(group)];
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_TYPES_H_
