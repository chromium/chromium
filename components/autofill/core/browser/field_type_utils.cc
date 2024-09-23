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

const FieldTypeSet& GetDatabaseStoredTypesOfAutofillProfile() {
  static constexpr FieldTypeSet stored_types{
      COMPANY_NAME,
      NAME_HONORIFIC_PREFIX,
      NAME_FIRST,
      NAME_MIDDLE,
      NAME_LAST_FIRST,
      NAME_LAST_CONJUNCTION,
      NAME_LAST_SECOND,
      NAME_LAST,
      NAME_FULL,
      ADDRESS_HOME_STREET_ADDRESS,
      ADDRESS_HOME_STREET_NAME,
      ADDRESS_HOME_STREET_LOCATION,
      ADDRESS_HOME_HOUSE_NUMBER,
      ADDRESS_HOME_HOUSE_NUMBER_AND_APT,
      ADDRESS_HOME_SUBPREMISE,
      ADDRESS_HOME_DEPENDENT_LOCALITY,
      ADDRESS_HOME_CITY,
      ADDRESS_HOME_STATE,
      ADDRESS_HOME_ZIP,
      ADDRESS_HOME_SORTING_CODE,
      ADDRESS_HOME_COUNTRY,
      ADDRESS_HOME_APT,
      ADDRESS_HOME_APT_NUM,
      ADDRESS_HOME_APT_TYPE,
      ADDRESS_HOME_FLOOR,
      ADDRESS_HOME_OVERFLOW,
      ADDRESS_HOME_LANDMARK,
      ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
      ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK,
      ADDRESS_HOME_BETWEEN_STREETS,
      ADDRESS_HOME_BETWEEN_STREETS_1,
      ADDRESS_HOME_BETWEEN_STREETS_2,
      ADDRESS_HOME_ADMIN_LEVEL2,
      ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY,
      EMAIL_ADDRESS,
      PHONE_HOME_WHOLE_NUMBER};
  return stored_types;
}

size_t NumberOfPossibleFieldTypesInGroup(const AutofillField& field,
                                         FieldTypeGroup group) {
  return base::ranges::count(field.possible_types(), group,
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
    case FieldTypeGroup::kPredictionImprovements:
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

}  // namespace autofill
