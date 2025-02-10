// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/entity_type.h"

#include <optional>

#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

namespace {

constexpr FieldType AttributeTypeNameToFieldType(AttributeTypeName a) {
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

// AttributeTypeNameToFieldType must be injective: distinct AttributeTypeNames
// must be mapped to distinct FieldTypes or to UNKNOWN_TYPE.
static_assert(
    std::ranges::all_of(DenseSet<AttributeType>::all(), [](AttributeType a) {
      return std::ranges::all_of(
          DenseSet<AttributeType>::all(), [&a](AttributeType b) {
            FieldType fta = AttributeTypeNameToFieldType(a.name());
            FieldType ftb = AttributeTypeNameToFieldType(b.name());
            return a == b || fta == UNKNOWN_TYPE || fta != ftb;
          });
    }));

}  // namespace

// static
std::optional<AttributeType> AttributeType::FromFieldType(FieldType type) {
  // This lookup table is the inverse of AttributeTypeNameToFieldType().
  static constexpr auto kTable = []() {
    std::array<std::optional<AttributeType>, MAX_VALID_FIELD_TYPE> arr{};
    for (AttributeType at : DenseSet<AttributeType>::all()) {
      FieldType ft = AttributeTypeNameToFieldType(at.name());
      CHECK(ft == UNKNOWN_TYPE || !arr[ft]);
      arr[ft] = ft != UNKNOWN_TYPE ? std::optional(at) : std::nullopt;
    }
    return arr;
  }();
  return 0 <= type && type < kTable.size() ? kTable[type] : std::nullopt;
}

// static
bool EntityType::ImportOrder(const EntityType& lhs, const EntityType& rhs) {
  auto rank = [](const EntityType& t) constexpr {
    // Lower values indicate a higher priority.
    // For a deterministic behavior, distinct types should have distinct ranks.
    switch (t.name()) {
      case EntityTypeName::kPassport:
        return 1;
      case EntityTypeName::kLoyaltyCard:
        return 2;
      case EntityTypeName::kCar:
        return 3;
      case EntityTypeName::kDriversLicense:
        return 4;
    }
  };
  return rank(lhs) < rank(rhs);
}

FieldType AttributeType::field_type() const {
  return AttributeTypeNameToFieldType(name_);
}

std::ostream& operator<<(std::ostream& os, AttributeType a) {
  return os << a.name_as_string();
}

std::ostream& operator<<(std::ostream& os, EntityType t) {
  return os << t.name_as_string();
}

}  // namespace autofill
