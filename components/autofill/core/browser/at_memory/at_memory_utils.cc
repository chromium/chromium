// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_utils.h"

#include <algorithm>

#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"

namespace autofill {

std::optional<AttributeType> GetPrimaryAttributeType(
    const EntityInstance& entity) {
  auto get_primary_attribute_name =
      [](EntityTypeName entity_type_name) -> AttributeTypeName {
    switch (entity_type_name) {
      case EntityTypeName::kVehicle:
        return AttributeTypeName::kVehiclePlateNumber;
      case EntityTypeName::kPassport:
        return AttributeTypeName::kPassportNumber;
      case EntityTypeName::kDriversLicense:
        return AttributeTypeName::kDriversLicenseNumber;
      case EntityTypeName::kOrder:
        return AttributeTypeName::kOrderId;
      case EntityTypeName::kNationalIdCard:
        return AttributeTypeName::kNationalIdCardNumber;
      case EntityTypeName::kKnownTravelerNumber:
        return AttributeTypeName::kKnownTravelerNumberNumber;
      case EntityTypeName::kRedressNumber:
        return AttributeTypeName::kRedressNumberNumber;
      case EntityTypeName::kFlightReservation:
        return AttributeTypeName::kFlightReservationFlightNumber;
      case EntityTypeName::kShipment:
        return AttributeTypeName::kShipmentTrackingNumber;
    }
  };

  AttributeType primary_type(get_primary_attribute_name(entity.type().name()));
  base::optional_ref<const AttributeInstance> primary_attr =
      entity.attribute(primary_type);
  if (primary_attr) {
    if (!primary_attr->GetCompleteRawInfo().empty()) {
      return primary_type;
    }
  }

  // Fallback to the first non-empty attribute if no primary attribute is found.
  if (auto it =
          std::ranges::find_if(entity.attributes(),
                               [](const AttributeInstance& attribute) {
                                 return !attribute.GetCompleteRawInfo().empty();
                               });
      it != entity.attributes().end()) {
    return it->type();
  }

  return std::nullopt;
}

}  // namespace autofill
