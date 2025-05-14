// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/field_type_utils.h"

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

size_t NumberOfPossibleFieldTypesInGroup(const AutofillField& field,
                                         FieldTypeGroup group) {
  return std::ranges::count(field.possible_types(), group,
                            GroupTypeOfFieldType);
}

bool FieldHasMeaningfulPossibleFieldTypes(const AutofillField& field) {
  // This function should only be invoked when the possible types have been
  // determined.
  DCHECK(!field.possible_types().empty());

  if (field.possible_types().contains_any({UNKNOWN_TYPE, EMPTY_TYPE})) {
    // If either UNKNOWN_TYPE or EMPTY_TYPE is present, there should be no other
    // type
    DCHECK_EQ(field.possible_types().size(), 1u);
    return false;
  }
  return true;
}

bool TypeOfFieldIsPossibleType(const AutofillField& field) {
  return field.possible_types().contains(field.Type().GetStorableType());
}

bool IsAddressType(FieldType type) {
  switch (GroupTypeOfFieldType(type)) {
    case FieldTypeGroup::kName:
    case FieldTypeGroup::kEmail:
    case FieldTypeGroup::kCompany:
    case FieldTypeGroup::kAddress:
    case FieldTypeGroup::kPhone:
      return true;
    case FieldTypeGroup::kNoGroup:
    case FieldTypeGroup::kUnfillable:
    case FieldTypeGroup::kCreditCard:
    case FieldTypeGroup::kUsernameField:
    case FieldTypeGroup::kPasswordField:
    case FieldTypeGroup::kTransaction:
    case FieldTypeGroup::kIban:
    case FieldTypeGroup::kStandaloneCvcField:
    case FieldTypeGroup::kAutofillAi:
    case FieldTypeGroup::kLoyaltyCard:
      return false;
  }
  NOTREACHED();
}

size_t AddressLineIndex(FieldType type) {
  static constexpr auto kAddressLineIndex =
      base::MakeFixedFlatMap<FieldType, size_t>({{ADDRESS_HOME_LINE1, 0},
                                                 {ADDRESS_HOME_LINE2, 1},
                                                 {ADDRESS_HOME_LINE3, 2}});
  if (kAddressLineIndex.contains(type)) {
    return kAddressLineIndex.at(type);
  }
  NOTREACHED();
}

size_t DetermineExpirationYearLength(FieldType assumed_field_type) {
  switch (assumed_field_type) {
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      return 2;
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return 4;
    default:
      NOTREACHED();
  }
}

bool IsAlternativeNameType(FieldType type) {
  return type == ALTERNATIVE_FULL_NAME || type == ALTERNATIVE_GIVEN_NAME ||
         type == ALTERNATIVE_FAMILY_NAME;
}

bool IsTagType(FieldType type) {
  switch (type) {
    case PASSPORT_NAME_TAG:
    case VEHICLE_OWNER_TAG:
    case DRIVERS_LICENSE_NAME_TAG:
      // Those types represent names, but don't specify which part of the name
      // does the field ask for. Such types need to be complemented by, for
      // example, NAME_FIRST to figure out the remaining information.
      return true;
    case NO_SERVER_DATA:
    case UNKNOWN_TYPE:
    case EMPTY_TYPE:
    case NAME_FIRST:
    case NAME_MIDDLE:
    case NAME_LAST:
    case NAME_MIDDLE_INITIAL:
    case NAME_FULL:
    case NAME_SUFFIX:
    case EMAIL_ADDRESS:
    case PHONE_HOME_NUMBER:
    case PHONE_HOME_CITY_CODE:
    case PHONE_HOME_COUNTRY_CODE:
    case PHONE_HOME_CITY_AND_NUMBER:
    case PHONE_HOME_WHOLE_NUMBER:
    case ADDRESS_HOME_LINE1:
    case ADDRESS_HOME_LINE2:
    case ADDRESS_HOME_APT_NUM:
    case ADDRESS_HOME_CITY:
    case ADDRESS_HOME_STATE:
    case ADDRESS_HOME_ZIP:
    case ADDRESS_HOME_COUNTRY:
    case CREDIT_CARD_NAME_FULL:
    case CREDIT_CARD_NUMBER:
    case CREDIT_CARD_EXP_MONTH:
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
    case CREDIT_CARD_TYPE:
    case CREDIT_CARD_VERIFICATION_CODE:
    case COMPANY_NAME:
    case FIELD_WITH_DEFAULT_VALUE:
    case MERCHANT_EMAIL_SIGNUP:
    case MERCHANT_PROMO_CODE:
    case PASSWORD:
    case ACCOUNT_CREATION_PASSWORD:
    case ADDRESS_HOME_STREET_ADDRESS:
    case ADDRESS_HOME_SORTING_CODE:
    case ADDRESS_HOME_DEPENDENT_LOCALITY:
    case ADDRESS_HOME_LINE3:
    case NOT_ACCOUNT_CREATION_PASSWORD:
    case USERNAME:
    case USERNAME_AND_EMAIL_ADDRESS:
    case NEW_PASSWORD:
    case PROBABLY_NEW_PASSWORD:
    case NOT_NEW_PASSWORD:
    case CREDIT_CARD_NAME_FIRST:
    case CREDIT_CARD_NAME_LAST:
    case PHONE_HOME_EXTENSION:
    case CONFIRMATION_PASSWORD:
    case AMBIGUOUS_TYPE:
    case SEARCH_TERM:
    case PRICE:
    case NOT_PASSWORD:
    case SINGLE_USERNAME:
    case NOT_USERNAME:
    case ADDRESS_HOME_STREET_NAME:
    case ADDRESS_HOME_HOUSE_NUMBER:
    case ADDRESS_HOME_SUBPREMISE:
    case ADDRESS_HOME_OTHER_SUBUNIT:
    case NAME_LAST_FIRST:
    case NAME_LAST_CONJUNCTION:
    case NAME_LAST_SECOND:
    case NAME_HONORIFIC_PREFIX:
    case ADDRESS_HOME_ADDRESS:
    case ADDRESS_HOME_ADDRESS_WITH_NAME:
    case ADDRESS_HOME_FLOOR:
    case PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
    case PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
    case PHONE_HOME_NUMBER_PREFIX:
    case PHONE_HOME_NUMBER_SUFFIX:
    case IBAN_VALUE:
    case CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
    case NUMERIC_QUANTITY:
    case ONE_TIME_CODE:
    case DELIVERY_INSTRUCTIONS:
    case ADDRESS_HOME_OVERFLOW:
    case ADDRESS_HOME_LANDMARK:
    case ADDRESS_HOME_OVERFLOW_AND_LANDMARK:
    case ADDRESS_HOME_ADMIN_LEVEL2:
    case ADDRESS_HOME_STREET_LOCATION:
    case ADDRESS_HOME_BETWEEN_STREETS:
    case ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK:
    case ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY:
    case ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK:
    case ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK:
    case ADDRESS_HOME_BETWEEN_STREETS_1:
    case ADDRESS_HOME_BETWEEN_STREETS_2:
    case ADDRESS_HOME_HOUSE_NUMBER_AND_APT:
    case SINGLE_USERNAME_FORGOT_PASSWORD:
    case ADDRESS_HOME_APT:
    case ADDRESS_HOME_APT_TYPE:
    case LOYALTY_MEMBERSHIP_ID:
    case SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES:
    case IMPROVED_PREDICTION:
    case ALTERNATIVE_FULL_NAME:
    case ALTERNATIVE_GIVEN_NAME:
    case ALTERNATIVE_FAMILY_NAME:
    case NAME_LAST_PREFIX:
    case NAME_LAST_CORE:
    case PASSPORT_NUMBER:
    case PASSPORT_ISSUING_COUNTRY:
    case PASSPORT_EXPIRATION_DATE:
    case PASSPORT_ISSUE_DATE:
    case LOYALTY_MEMBERSHIP_PROGRAM:
    case LOYALTY_MEMBERSHIP_PROVIDER:
    case VEHICLE_LICENSE_PLATE:
    case VEHICLE_VIN:
    case VEHICLE_MAKE:
    case VEHICLE_MODEL:
    case DRIVERS_LICENSE_REGION:
    case DRIVERS_LICENSE_NUMBER:
    case DRIVERS_LICENSE_EXPIRATION_DATE:
    case DRIVERS_LICENSE_ISSUE_DATE:
    case VEHICLE_YEAR:
    case VEHICLE_PLATE_STATE:
    case EMAIL_OR_LOYALTY_MEMBERSHIP_ID:
    case MAX_VALID_FIELD_TYPE:
      return false;
  }
  NOTREACHED();
}

}  // namespace autofill
