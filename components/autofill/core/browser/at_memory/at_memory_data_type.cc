// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"

#include <optional>

#include "components/accessibility_annotator/annotation_reducer/query_intent_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

std::optional<AtMemoryDataType> ToAtMemoryDataType(
    annotation_reducer::QueryIntentType query_intent_type) {
  switch (query_intent_type) {
    case annotation_reducer::QueryIntentType::kNameFull:
      return NAME_FULL;
    case annotation_reducer::QueryIntentType::kAddressFull:
      return ADDRESS_HOME_ADDRESS;
    case annotation_reducer::QueryIntentType::kAddressStreetAddress:
      return ADDRESS_HOME_STREET_ADDRESS;
    case annotation_reducer::QueryIntentType::kAddressCity:
      return ADDRESS_HOME_CITY;
    case annotation_reducer::QueryIntentType::kAddressState:
      return ADDRESS_HOME_STATE;
    case annotation_reducer::QueryIntentType::kAddressZip:
      return ADDRESS_HOME_ZIP;
    case annotation_reducer::QueryIntentType::kAddressCountry:
      return ADDRESS_HOME_COUNTRY;
    case annotation_reducer::QueryIntentType::kPhone:
      return PHONE_HOME_WHOLE_NUMBER;
    case annotation_reducer::QueryIntentType::kEmail:
      return EMAIL_ADDRESS;
    case annotation_reducer::QueryIntentType::kIban:
      return IBAN_VALUE;
    case annotation_reducer::QueryIntentType::kVehicle:
      return EntityType(EntityTypeName::kVehicle);
    case annotation_reducer::QueryIntentType::kVehiclePlateNumber:
      return AttributeType(AttributeTypeName::kVehiclePlateNumber);
    case annotation_reducer::QueryIntentType::kVehicleVin:
      return AttributeType(AttributeTypeName::kVehicleVin);
    case annotation_reducer::QueryIntentType::kPassportFull:
      return EntityType(EntityTypeName::kPassport);
    case annotation_reducer::QueryIntentType::kFlightReservationFull:
      return EntityType(EntityTypeName::kFlightReservation);
    case annotation_reducer::QueryIntentType::kNationalIdCardFull:
      return EntityType(EntityTypeName::kNationalIdCard);
    case annotation_reducer::QueryIntentType::kRedressNumberFull:
      return EntityType(EntityTypeName::kRedressNumber);
    case annotation_reducer::QueryIntentType::kKnownTravelerNumberFull:
      return EntityType(EntityTypeName::kKnownTravelerNumber);
    case annotation_reducer::QueryIntentType::kDriversLicenseFull:
      return EntityType(EntityTypeName::kDriversLicense);
    case annotation_reducer::QueryIntentType::kUnknown:
      return std::nullopt;
  }
}

}  // namespace autofill
