// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/field_type_utils.h"

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

size_t NumberOfPossibleFieldTypesInGroup(const AutofillField& field,
                                         FieldTypeGroup group) {
  return base::ranges::count(field.possible_types(), group,
                             GroupTypeOfServerFieldType);
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

bool IsStreetNameOrHouseNumberType(const ServerFieldType type) {
  return type == ADDRESS_HOME_STREET_NAME || type == ADDRESS_HOME_HOUSE_NUMBER;
}

bool IsAddressType(const AutofillType& type) {
  switch (type.group()) {
    case FieldTypeGroup::kName:
    case FieldTypeGroup::kEmail:
    case FieldTypeGroup::kCompany:
    case FieldTypeGroup::kAddress:
    case FieldTypeGroup::kPhone:
    case FieldTypeGroup::kBirthdateField:
      return true;
    case FieldTypeGroup::kNoGroup:
    case FieldTypeGroup::kUnfillable:
    case FieldTypeGroup::kCreditCard:
    case FieldTypeGroup::kUsernameField:
    case FieldTypeGroup::kPasswordField:
    case FieldTypeGroup::kTransaction:
    case FieldTypeGroup::kIban:
      return false;
  }
  NOTREACHED_NORETURN();
}

size_t AddressLineIndex(ServerFieldType type) {
  static constexpr auto kAddressLineIndex =
      base::MakeFixedFlatMap<ServerFieldType, size_t>(
          {{ADDRESS_HOME_LINE1, 0},
           {ADDRESS_HOME_LINE2, 1},
           {ADDRESS_HOME_LINE3, 2}});
  if (kAddressLineIndex.contains(type)) {
    return kAddressLineIndex.at(type);
  }
  NOTREACHED_NORETURN();
}

size_t DetermineExpirationYearLength(ServerFieldType assumed_field_type) {
  switch (assumed_field_type) {
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      return 2;
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return 4;
    default:
      NOTREACHED_NORETURN();
  }
}

}  // namespace autofill
