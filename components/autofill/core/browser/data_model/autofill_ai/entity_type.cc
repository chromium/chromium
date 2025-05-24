// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

#include <optional>

#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

// AttributeType::field_type() must be injective: distinct AttributeTypes must
// be mapped to distinct FieldTypes.
static_assert(
    std::ranges::all_of(DenseSet<AttributeType>::all(), [](AttributeType a) {
      return std::ranges::all_of(
          DenseSet<AttributeType>::all(), [&a](AttributeType b) {
            return a == b || a.field_type() != b.field_type();
          });
    }));

// static
std::optional<AttributeType> AttributeType::FromFieldType(FieldType type) {
  // This lookup table is the inverse of AttributeType::field_type().
  static constexpr auto kTable = []() {
    std::array<std::optional<AttributeType>, MAX_VALID_FIELD_TYPE> arr{};
    for (AttributeType at : DenseSet<AttributeType>::all()) {
      arr[at.field_type()] = at;
    }
    return arr;
  }();
  return 0 <= type && type < kTable.size() ? kTable[type] : std::nullopt;
}

std::u16string AttributeType::GetNameForI18n() const {
  return l10n_util::GetStringUTF16([&] {
    switch (name()) {
      case AttributeTypeName::kPassportName:
        return IDS_AUTOFILL_AI_PASSPORT_NAME_ATTRIBUTE_NAME;
      case AttributeTypeName::kPassportNumber:
        return IDS_AUTOFILL_AI_PASSPORT_NUMBER_ATTRIBUTE_NAME;
      case AttributeTypeName::kPassportCountry:
        return IDS_AUTOFILL_AI_PASSPORT_COUNTRY_ATTRIBUTE_NAME;
      case AttributeTypeName::kPassportExpirationDate:
        return IDS_AUTOFILL_AI_PASSPORT_EXPIRATION_DATE_ATTRIBUTE_NAME;
      case AttributeTypeName::kPassportIssueDate:
        return IDS_AUTOFILL_AI_PASSPORT_ISSUE_DATE_ATTRIBUTE_NAME;
      case AttributeTypeName::kVehicleOwner:
        return IDS_AUTOFILL_AI_VEHICLE_OWNER_ATTRIBUTE_NAME;
      case AttributeTypeName::kVehiclePlateNumber:
        return IDS_AUTOFILL_AI_VEHICLE_PLATE_NUMBER_ATTRIBUTE_NAME;
      case AttributeTypeName::kVehiclePlateState:
        return IDS_AUTOFILL_AI_VEHICLE_PLATE_STATE_ATTRIBUTE_NAME;
      case AttributeTypeName::kVehicleVin:
        return IDS_AUTOFILL_AI_VEHICLE_VEHICLE_IDENTIFICATION_NUMBER_ATTRIBUTE_NAME;
      case AttributeTypeName::kVehicleMake:
        return IDS_AUTOFILL_AI_VEHICLE_MAKE_ATTRIBUTE_NAME;
      case AttributeTypeName::kVehicleModel:
        return IDS_AUTOFILL_AI_VEHICLE_MODEL_ATTRIBUTE_NAME;
      case AttributeTypeName::kVehicleYear:
        return IDS_AUTOFILL_AI_VEHICLE_YEAR_ATTRIBUTE_NAME;
      case AttributeTypeName::kDriversLicenseName:
        return IDS_AUTOFILL_AI_DRIVERS_LICENSE_NAME_ATTRIBUTE_NAME;
      case AttributeTypeName::kDriversLicenseState:
        return IDS_AUTOFILL_AI_DRIVERS_LICENSE_STATE_ATTRIBUTE_NAME;
      case AttributeTypeName::kDriversLicenseNumber:
        return IDS_AUTOFILL_AI_DRIVERS_LICENSE_NUMBER_ATTRIBUTE_NAME;
      case AttributeTypeName::kDriversLicenseExpirationDate:
        return IDS_AUTOFILL_AI_DRIVERS_LICENSE_EXPIRATION_DATE_ATTRIBUTE_NAME;
      case AttributeTypeName::kDriversLicenseIssueDate:
        return IDS_AUTOFILL_AI_DRIVERS_LICENSE_ISSUE_DATE_ATTRIBUTE_NAME;
    }
    NOTREACHED();
  }());
}

// static
bool EntityType::ImportOrder(const EntityType& lhs, const EntityType& rhs) {
  auto rank = [](const EntityType& t) constexpr {
    // Lower values indicate a higher priority.
    // For a deterministic behavior, distinct types should have distinct ranks.
    switch (t.name()) {
      case EntityTypeName::kPassport:
        return 1;
      case EntityTypeName::kVehicle:
        return 2;
      case EntityTypeName::kDriversLicense:
        return 3;
    }
  };
  return rank(lhs) < rank(rhs);
}

std::u16string EntityType::GetNameForI18n() const {
  switch (name()) {
    case EntityTypeName::kPassport:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_PASSPORT_ENTITY_NAME);
    case EntityTypeName::kVehicle:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_VEHICLE_ENTITY_NAME);
    case EntityTypeName::kDriversLicense:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_DRIVERS_LICENSE_ENTITY_NAME);
  }
  NOTREACHED();
}

std::optional<EntityTypeName> ToSafeEntityTypeName(
    std::underlying_type_t<EntityTypeName> raw_value) {
  // We rely here and elsewhere (such as in iteration over
  // `DenseSet<EntityType>::all()`) on the fact that `EntityTypeName` is
  // auto-generated and its value range is contiguous. If it were not, this
  // check would not be sufficient.
  if (raw_value < 0 ||
      raw_value > base::to_underlying(EntityTypeName::kMaxValue)) {
    return std::nullopt;
  }
  return EntityTypeName(raw_value);
}

std::ostream& operator<<(std::ostream& os, AttributeType a) {
  return os << a.name_as_string();
}

std::ostream& operator<<(std::ostream& os, EntityType t) {
  return os << t.name_as_string();
}

}  // namespace autofill
