// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/entity_type.h"

#include <optional>

#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

std::optional<AttributeType> AttributeType::FromFieldType(FieldType type) {
  switch (type) {
    case PASSPORT_NAME_TAG:
      return AttributeType(AttributeTypeName::kPassportName);
    case PASSPORT_NUMBER:
      return AttributeType(AttributeTypeName::kPassportNumber);
    case PASSPORT_ISSUING_COUNTRY_TAG:
      return AttributeType(AttributeTypeName::kPassportCountry);
    case PASSPORT_EXPIRATION_DATE_TAG:
      return AttributeType(AttributeTypeName::kPassportExpiryDate);
    case PASSPORT_ISSUE_DATE_TAG:
      return AttributeType(AttributeTypeName::kPassportIssueDate);
    case LOYALTY_MEMBERSHIP_PROGRAM:
      return AttributeType(AttributeTypeName::kLoyaltyCardProgram);
    case LOYALTY_MEMBERSHIP_PROVIDER:
      return AttributeType(AttributeTypeName::kLoyaltyCardProvider);
    case LOYALTY_MEMBERSHIP_ID:
      return AttributeType(AttributeTypeName::kLoyaltyCardMemberId);
    default:
      return std::nullopt;
  }
}

FieldType AttributeTypeNameToFieldType(AttributeTypeName a) {
  switch (a) {
    case AttributeTypeName::kPassportName:
      return PASSPORT_NAME_TAG;
    case AttributeTypeName::kPassportNumber:
      return PASSPORT_NUMBER;
    case AttributeTypeName::kPassportCountry:
      return PASSPORT_ISSUING_COUNTRY_TAG;
    case AttributeTypeName::kPassportExpiryDate:
      return PASSPORT_EXPIRATION_DATE_TAG;
    case AttributeTypeName::kPassportIssueDate:
      return PASSPORT_ISSUE_DATE_TAG;
    case AttributeTypeName::kLoyaltyCardProgram:
      return LOYALTY_MEMBERSHIP_PROGRAM;
    case AttributeTypeName::kLoyaltyCardProvider:
      return LOYALTY_MEMBERSHIP_PROVIDER;
    case AttributeTypeName::kLoyaltyCardMemberId:
      return LOYALTY_MEMBERSHIP_ID;
    case AttributeTypeName::kCarOwner:
    case AttributeTypeName::kCarLicensePlate:
    case AttributeTypeName::kCarRegistration:
    case AttributeTypeName::kCarMake:
    case AttributeTypeName::kCarModel:
      return UNKNOWN_TYPE;
    case AttributeTypeName::kDriversLicenseName:
    case AttributeTypeName::kDriversLicenseRegion:
    case AttributeTypeName::kDriversLicenseNumber:
    case AttributeTypeName::kDriversLicenseExpirationDate:
    case AttributeTypeName::kDriversLicenseIssueDate:
      return UNKNOWN_TYPE;
  }
  NOTREACHED();
}

EntityType AttributeType::entity_type() const {
  return EntityType(AttributeTypeNameToEntityTypeName(name_));
}

std::ostream& operator<<(std::ostream& os, AttributeType a) {
  return os << a.name_as_string();
}

std::ostream& operator<<(std::ostream& os, EntityType t) {
  return os << t.name_as_string();
}

}  // namespace autofill
