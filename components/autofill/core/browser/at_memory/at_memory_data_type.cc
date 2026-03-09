// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"

#include <optional>

#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

std::optional<AtMemoryDataType> ToAtMemoryDataType(
    accessibility_annotator::QueryIntentType query_intent_type) {
#define INTENT_TO_FIELD_TYPE(intent, field_type)         \
  case accessibility_annotator::QueryIntentType::intent: \
    return field_type
#define INTENT_TO_ENTITY_TYPE(intent, entity_type)       \
  case accessibility_annotator::QueryIntentType::intent: \
    return EntityType(EntityTypeName::entity_type)
#define INTENT_TO_ATTRIBUTE_TYPE(intent_and_attribute_type)                 \
  case accessibility_annotator::QueryIntentType::intent_and_attribute_type: \
    return AttributeType(AttributeTypeName::intent_and_attribute_type)

  switch (query_intent_type) {
    INTENT_TO_FIELD_TYPE(kNameFull, NAME_FULL);
    INTENT_TO_FIELD_TYPE(kAddressFull, ADDRESS_HOME_ADDRESS);
    INTENT_TO_FIELD_TYPE(kAddressStreetAddress, ADDRESS_HOME_STREET_ADDRESS);
    INTENT_TO_FIELD_TYPE(kAddressCity, ADDRESS_HOME_CITY);
    INTENT_TO_FIELD_TYPE(kAddressState, ADDRESS_HOME_STATE);
    INTENT_TO_FIELD_TYPE(kAddressZip, ADDRESS_HOME_ZIP);
    INTENT_TO_FIELD_TYPE(kAddressCountry, ADDRESS_HOME_COUNTRY);
    INTENT_TO_FIELD_TYPE(kPhone, PHONE_HOME_WHOLE_NUMBER);
    INTENT_TO_FIELD_TYPE(kEmail, EMAIL_ADDRESS);
    INTENT_TO_FIELD_TYPE(kCompanyName, COMPANY_NAME);
    INTENT_TO_FIELD_TYPE(kIban, IBAN_VALUE);
    INTENT_TO_ENTITY_TYPE(kVehicle, kVehicle);
    INTENT_TO_ATTRIBUTE_TYPE(kVehicleMake);
    INTENT_TO_ATTRIBUTE_TYPE(kVehicleModel);
    INTENT_TO_ATTRIBUTE_TYPE(kVehicleYear);
    INTENT_TO_ATTRIBUTE_TYPE(kVehicleOwner);
    INTENT_TO_ATTRIBUTE_TYPE(kVehiclePlateNumber);
    INTENT_TO_ATTRIBUTE_TYPE(kVehiclePlateState);
    INTENT_TO_ATTRIBUTE_TYPE(kVehicleVin);
    INTENT_TO_ENTITY_TYPE(kPassportFull, kPassport);
    INTENT_TO_ATTRIBUTE_TYPE(kPassportName);
    INTENT_TO_ATTRIBUTE_TYPE(kPassportCountry);
    INTENT_TO_ATTRIBUTE_TYPE(kPassportNumber);
    INTENT_TO_ATTRIBUTE_TYPE(kPassportIssueDate);
    INTENT_TO_ATTRIBUTE_TYPE(kPassportExpirationDate);
    INTENT_TO_ENTITY_TYPE(kFlightReservationFull, kFlightReservation);
    INTENT_TO_ATTRIBUTE_TYPE(kFlightReservationFlightNumber);
    INTENT_TO_ATTRIBUTE_TYPE(kFlightReservationTicketNumber);
    INTENT_TO_ATTRIBUTE_TYPE(kFlightReservationConfirmationCode);
    INTENT_TO_ATTRIBUTE_TYPE(kFlightReservationPassengerName);
    INTENT_TO_ATTRIBUTE_TYPE(kFlightReservationDepartureAirport);
    INTENT_TO_ATTRIBUTE_TYPE(kFlightReservationArrivalAirport);
    INTENT_TO_ATTRIBUTE_TYPE(kFlightReservationDepartureDate);
    INTENT_TO_ENTITY_TYPE(kNationalIdCardFull, kNationalIdCard);
    INTENT_TO_ATTRIBUTE_TYPE(kNationalIdCardName);
    INTENT_TO_ATTRIBUTE_TYPE(kNationalIdCardCountry);
    INTENT_TO_ATTRIBUTE_TYPE(kNationalIdCardNumber);
    INTENT_TO_ATTRIBUTE_TYPE(kNationalIdCardIssueDate);
    INTENT_TO_ATTRIBUTE_TYPE(kNationalIdCardExpirationDate);
    INTENT_TO_ENTITY_TYPE(kRedressNumberFull, kRedressNumber);
    INTENT_TO_ATTRIBUTE_TYPE(kRedressNumberName);
    INTENT_TO_ATTRIBUTE_TYPE(kRedressNumberNumber);
    INTENT_TO_ENTITY_TYPE(kKnownTravelerNumberFull, kKnownTravelerNumber);
    INTENT_TO_ATTRIBUTE_TYPE(kKnownTravelerNumberName);
    INTENT_TO_ATTRIBUTE_TYPE(kKnownTravelerNumberNumber);
    INTENT_TO_ATTRIBUTE_TYPE(kKnownTravelerNumberExpirationDate);
    INTENT_TO_ENTITY_TYPE(kDriversLicenseFull, kDriversLicense);
    INTENT_TO_ATTRIBUTE_TYPE(kDriversLicenseName);
    INTENT_TO_ATTRIBUTE_TYPE(kDriversLicenseState);
    INTENT_TO_ATTRIBUTE_TYPE(kDriversLicenseNumber);
    INTENT_TO_ATTRIBUTE_TYPE(kDriversLicenseIssueDate);
    INTENT_TO_ATTRIBUTE_TYPE(kDriversLicenseExpirationDate);
    INTENT_TO_ENTITY_TYPE(kOrderFull, kOrder);
    INTENT_TO_ATTRIBUTE_TYPE(kOrderId);
    INTENT_TO_ATTRIBUTE_TYPE(kOrderAccount);
    INTENT_TO_ATTRIBUTE_TYPE(kOrderDate);
    INTENT_TO_ATTRIBUTE_TYPE(kOrderMerchantName);
    INTENT_TO_ATTRIBUTE_TYPE(kOrderMerchantDomain);
    INTENT_TO_ATTRIBUTE_TYPE(kOrderProductNames);
    INTENT_TO_ATTRIBUTE_TYPE(kOrderGrandTotal);
    case accessibility_annotator::QueryIntentType::kUnknown:
      return std::nullopt;
  }

#undef INTENT_TO_ATTRIBUTE_TYPE
#undef INTENT_TO_ENTITY_TYPE
#undef INTENT_TO_FIELD_TYPE
}

}  // namespace autofill
