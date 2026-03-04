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
    case annotation_reducer::QueryIntentType::kVehicleMake:
      return AttributeType(AttributeTypeName::kVehicleMake);
    case annotation_reducer::QueryIntentType::kVehicleModel:
      return AttributeType(AttributeTypeName::kVehicleModel);
    case annotation_reducer::QueryIntentType::kVehicleYear:
      return AttributeType(AttributeTypeName::kVehicleYear);
    case annotation_reducer::QueryIntentType::kVehicleOwner:
      return AttributeType(AttributeTypeName::kVehicleOwner);
    case annotation_reducer::QueryIntentType::kVehiclePlateNumber:
      return AttributeType(AttributeTypeName::kVehiclePlateNumber);
    case annotation_reducer::QueryIntentType::kVehiclePlateState:
      return AttributeType(AttributeTypeName::kVehiclePlateState);
    case annotation_reducer::QueryIntentType::kVehicleVin:
      return AttributeType(AttributeTypeName::kVehicleVin);
    case annotation_reducer::QueryIntentType::kPassportFull:
      return EntityType(EntityTypeName::kPassport);
    case annotation_reducer::QueryIntentType::kPassportName:
      return AttributeType(AttributeTypeName::kPassportName);
    case annotation_reducer::QueryIntentType::kPassportCountry:
      return AttributeType(AttributeTypeName::kPassportCountry);
    case annotation_reducer::QueryIntentType::kPassportNumber:
      return AttributeType(AttributeTypeName::kPassportNumber);
    case annotation_reducer::QueryIntentType::kPassportIssueDate:
      return AttributeType(AttributeTypeName::kPassportIssueDate);
    case annotation_reducer::QueryIntentType::kPassportExpirationDate:
      return AttributeType(AttributeTypeName::kPassportExpirationDate);
    case annotation_reducer::QueryIntentType::kFlightReservationFull:
      return EntityType(EntityTypeName::kFlightReservation);
    case annotation_reducer::QueryIntentType::kFlightReservationFlightNumber:
      return AttributeType(AttributeTypeName::kFlightReservationFlightNumber);
    case annotation_reducer::QueryIntentType::kFlightReservationTicketNumber:
      return AttributeType(AttributeTypeName::kFlightReservationTicketNumber);
    case annotation_reducer::QueryIntentType::
        kFlightReservationConfirmationCode:
      return AttributeType(
          AttributeTypeName::kFlightReservationConfirmationCode);
    case annotation_reducer::QueryIntentType::kFlightReservationPassengerName:
      return AttributeType(AttributeTypeName::kFlightReservationPassengerName);
    case annotation_reducer::QueryIntentType::
        kFlightReservationDepartureAirport:
      return AttributeType(
          AttributeTypeName::kFlightReservationDepartureAirport);
    case annotation_reducer::QueryIntentType::kFlightReservationArrivalAirport:
      return AttributeType(AttributeTypeName::kFlightReservationArrivalAirport);
    case annotation_reducer::QueryIntentType::kFlightReservationDepartureDate:
      return AttributeType(AttributeTypeName::kFlightReservationDepartureDate);
    case annotation_reducer::QueryIntentType::kNationalIdCardFull:
      return EntityType(EntityTypeName::kNationalIdCard);
    case annotation_reducer::QueryIntentType::kNationalIdCardName:
      return AttributeType(AttributeTypeName::kNationalIdCardName);
    case annotation_reducer::QueryIntentType::kNationalIdCardCountry:
      return AttributeType(AttributeTypeName::kNationalIdCardCountry);
    case annotation_reducer::QueryIntentType::kNationalIdCardNumber:
      return AttributeType(AttributeTypeName::kNationalIdCardNumber);
    case annotation_reducer::QueryIntentType::kNationalIdCardIssueDate:
      return AttributeType(AttributeTypeName::kNationalIdCardIssueDate);
    case annotation_reducer::QueryIntentType::kNationalIdCardExpirationDate:
      return AttributeType(AttributeTypeName::kNationalIdCardExpirationDate);
    case annotation_reducer::QueryIntentType::kRedressNumberFull:
      return EntityType(EntityTypeName::kRedressNumber);
    case annotation_reducer::QueryIntentType::kRedressNumberName:
      return AttributeType(AttributeTypeName::kRedressNumberName);
    case annotation_reducer::QueryIntentType::kRedressNumberNumber:
      return AttributeType(AttributeTypeName::kRedressNumberNumber);
    case annotation_reducer::QueryIntentType::kKnownTravelerNumberFull:
      return EntityType(EntityTypeName::kKnownTravelerNumber);
    case annotation_reducer::QueryIntentType::kKnownTravelerNumberName:
      return AttributeType(AttributeTypeName::kKnownTravelerNumberName);
    case annotation_reducer::QueryIntentType::kKnownTravelerNumberNumber:
      return AttributeType(AttributeTypeName::kKnownTravelerNumberNumber);
    case annotation_reducer::QueryIntentType::
        kKnownTravelerNumberExpirationDate:
      return AttributeType(
          AttributeTypeName::kKnownTravelerNumberExpirationDate);
    case annotation_reducer::QueryIntentType::kDriversLicenseFull:
      return EntityType(EntityTypeName::kDriversLicense);
    case annotation_reducer::QueryIntentType::kDriversLicenseName:
      return AttributeType(AttributeTypeName::kDriversLicenseName);
    case annotation_reducer::QueryIntentType::kDriversLicenseState:
      return AttributeType(AttributeTypeName::kDriversLicenseState);
    case annotation_reducer::QueryIntentType::kDriversLicenseNumber:
      return AttributeType(AttributeTypeName::kDriversLicenseNumber);
    case annotation_reducer::QueryIntentType::kDriversLicenseIssueDate:
      return AttributeType(AttributeTypeName::kDriversLicenseIssueDate);
    case annotation_reducer::QueryIntentType::kDriversLicenseExpirationDate:
      return AttributeType(AttributeTypeName::kDriversLicenseExpirationDate);
    case annotation_reducer::QueryIntentType::kOrderFull:
      return EntityType(EntityTypeName::kOrder);
    case annotation_reducer::QueryIntentType::kOrderId:
      return AttributeType(AttributeTypeName::kOrderId);
    case annotation_reducer::QueryIntentType::kOrderAccount:
      return AttributeType(AttributeTypeName::kOrderAccount);
    case annotation_reducer::QueryIntentType::kOrderDate:
      return AttributeType(AttributeTypeName::kOrderDate);
    case annotation_reducer::QueryIntentType::kOrderMerchantName:
      return AttributeType(AttributeTypeName::kOrderMerchantName);
    case annotation_reducer::QueryIntentType::kOrderMerchantDomain:
      return AttributeType(AttributeTypeName::kOrderMerchantDomain);
    case annotation_reducer::QueryIntentType::kOrderProductNames:
      return AttributeType(AttributeTypeName::kOrderProductNames);
    case annotation_reducer::QueryIntentType::kOrderGrandTotal:
      return AttributeType(AttributeTypeName::kOrderGrandTotal);
    case annotation_reducer::QueryIntentType::kUnknown:
      return std::nullopt;
  }
}

}  // namespace autofill
