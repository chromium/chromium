// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/entity_type.h"

#include <optional>

#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// AttributeType::field_type() must be injective: distinct AttributeTypes must
// be mapped to distinct FieldTypes or to UNKNOWN_TYPE.
static_assert(
    std::ranges::all_of(DenseSet<AttributeType>::all(), [](AttributeType a) {
      return std::ranges::all_of(
          DenseSet<AttributeType>::all(), [&a](AttributeType b) {
            return a == b || a.field_type() == UNKNOWN_TYPE ||
                   a.field_type() != b.field_type();
          });
    }));

// static
std::optional<AttributeType> AttributeType::FromFieldType(FieldType type) {
  // This lookup table is the inverse of AttributeType::field_type().
  static constexpr auto kTable = []() {
    std::array<std::optional<AttributeType>, MAX_VALID_FIELD_TYPE> arr{};
    for (AttributeType at : DenseSet<AttributeType>::all()) {
      FieldType ft = at.field_type();
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

std::ostream& operator<<(std::ostream& os, AttributeType a) {
  return os << a.name_as_string();
}

std::ostream& operator<<(std::ostream& os, EntityType t) {
  return os << t.name_as_string();
}

}  // namespace autofill
