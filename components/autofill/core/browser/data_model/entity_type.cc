// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/entity_type.h"

#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

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
    case AttributeTypeName::kPassportPlaceOfBirth:
      return PASSPORT_COUNTRY_OF_BIRTH_TAG;
    case AttributeTypeName::kLoyaltyCardProgram:
      return LOYALTY_CARD_PROGRAM;
    case AttributeTypeName::kLoyaltyCardProvider:
      return LOYALTY_CARD_PROVIDER;
    case AttributeTypeName::kLoyaltyCardMemberId:
      return LOYALTY_CARD_MEMBER_ID;
  }
  NOTREACHED();
}

EntityType AttributeType::entity_type() const {
  return EntityType(AttributeTypeNameToEntityTypeName(name_));
}

std::ostream& operator<<(std::ostream& os, EntityType t) {
  return os << base::to_underlying(t.name());
}

std::ostream& operator<<(std::ostream& os, AttributeType t) {
  return os << base::to_underlying(t.name());
}

}  // namespace autofill
