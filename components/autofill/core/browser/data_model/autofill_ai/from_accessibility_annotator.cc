// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/from_accessibility_annotator.h"

#include <string>
#include <variant>

#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

namespace autofill {

namespace aa = accessibility_annotator;

aa::MemoryDataType AttributeTypeToMemoryDataType(AttributeType type) {
#define ATTRIBUTE_TO_QUERY_INTENT(name) \
  case AttributeTypeName::name:         \
    return aa::MemoryDataType::name

  switch (type.name()) {
    ATTRIBUTE_TO_QUERY_INTENT(kDriversLicenseName);
    ATTRIBUTE_TO_QUERY_INTENT(kDriversLicenseState);
    ATTRIBUTE_TO_QUERY_INTENT(kDriversLicenseNumber);
    ATTRIBUTE_TO_QUERY_INTENT(kDriversLicenseIssueDate);
    ATTRIBUTE_TO_QUERY_INTENT(kDriversLicenseExpirationDate);
    ATTRIBUTE_TO_QUERY_INTENT(kFlightReservationPassengerName);
    ATTRIBUTE_TO_QUERY_INTENT(kFlightReservationFlightNumber);
    ATTRIBUTE_TO_QUERY_INTENT(kFlightReservationTicketNumber);
    ATTRIBUTE_TO_QUERY_INTENT(kFlightReservationConfirmationCode);
    ATTRIBUTE_TO_QUERY_INTENT(kFlightReservationDepartureAirport);
    ATTRIBUTE_TO_QUERY_INTENT(kFlightReservationArrivalAirport);
    ATTRIBUTE_TO_QUERY_INTENT(kFlightReservationDepartureDate);
    ATTRIBUTE_TO_QUERY_INTENT(kKnownTravelerNumberName);
    ATTRIBUTE_TO_QUERY_INTENT(kKnownTravelerNumberNumber);
    ATTRIBUTE_TO_QUERY_INTENT(kKnownTravelerNumberExpirationDate);
    ATTRIBUTE_TO_QUERY_INTENT(kNationalIdCardName);
    ATTRIBUTE_TO_QUERY_INTENT(kNationalIdCardCountry);
    ATTRIBUTE_TO_QUERY_INTENT(kNationalIdCardNumber);
    ATTRIBUTE_TO_QUERY_INTENT(kNationalIdCardIssueDate);
    ATTRIBUTE_TO_QUERY_INTENT(kNationalIdCardExpirationDate);
    ATTRIBUTE_TO_QUERY_INTENT(kOrderAccount);
    ATTRIBUTE_TO_QUERY_INTENT(kOrderDate);
    ATTRIBUTE_TO_QUERY_INTENT(kOrderId);
    ATTRIBUTE_TO_QUERY_INTENT(kOrderMerchantDomain);
    ATTRIBUTE_TO_QUERY_INTENT(kOrderMerchantName);
    ATTRIBUTE_TO_QUERY_INTENT(kOrderProductNames);
    ATTRIBUTE_TO_QUERY_INTENT(kPassportName);
    ATTRIBUTE_TO_QUERY_INTENT(kPassportCountry);
    ATTRIBUTE_TO_QUERY_INTENT(kPassportNumber);
    ATTRIBUTE_TO_QUERY_INTENT(kPassportIssueDate);
    ATTRIBUTE_TO_QUERY_INTENT(kPassportExpirationDate);
    ATTRIBUTE_TO_QUERY_INTENT(kRedressNumberName);
    ATTRIBUTE_TO_QUERY_INTENT(kRedressNumberNumber);
    ATTRIBUTE_TO_QUERY_INTENT(kVehicleOwner);
    ATTRIBUTE_TO_QUERY_INTENT(kVehiclePlateNumber);
    ATTRIBUTE_TO_QUERY_INTENT(kVehiclePlateState);
    ATTRIBUTE_TO_QUERY_INTENT(kVehicleVin);
    ATTRIBUTE_TO_QUERY_INTENT(kVehicleMake);
    ATTRIBUTE_TO_QUERY_INTENT(kVehicleModel);
    ATTRIBUTE_TO_QUERY_INTENT(kVehicleYear);
    ATTRIBUTE_TO_QUERY_INTENT(kShipmentCarrierName);
    ATTRIBUTE_TO_QUERY_INTENT(kShipmentCarrierDomain);
    ATTRIBUTE_TO_QUERY_INTENT(kShipmentTrackingNumber);
    ATTRIBUTE_TO_QUERY_INTENT(kShipmentShippedDate);
    case AttributeTypeName::kShipmentOrderIds:
      return aa::MemoryDataType::kShipmentAssociatedOrderId;
    case AttributeTypeName::kShipmentOrderDates:
    case AttributeTypeName::kShipmentMerchantName:
    case AttributeTypeName::kShipmentProductNames:
    case AttributeTypeName::kShipmentDeliveryZipCode:
      // TODO(crbug.com/484094746): Map `delivery_address` to
      // `kShipmentDeliveryZipCode`. Since `delivery_address` is a
      // `std::string`, it's unclear how we can process this (here and in
      // general).
      return aa::MemoryDataType::kUnknown;
  }
#undef ATTRIBUTE_TO_QUERY_INTENT
  return aa::MemoryDataType::kUnknown;
}

std::u16string GetMemoryDataTypeNameForI18n(aa::MemoryDataType type) {
  switch (type) {
    case aa::MemoryDataType::kUnknown:
      return u"";
    // Field types:
    // TODO(crbug.com/481979475): Use internationalization for these strings.
    case aa::MemoryDataType::kNameFull:
      return u"Name";
    case aa::MemoryDataType::kAddressFull:
      return u"Address";
    case aa::MemoryDataType::kAddressStreetAddress:
      return u"Street address";
    case aa::MemoryDataType::kAddressCity:
      return u"City";
    case aa::MemoryDataType::kAddressState:
      return u"State";
    case aa::MemoryDataType::kAddressZip:
      return u"Zip";
    case aa::MemoryDataType::kAddressCountry:
      return u"Country";
    case aa::MemoryDataType::kPhone:
      return u"Phone";
    case aa::MemoryDataType::kEmail:
      return u"Email";
    case aa::MemoryDataType::kCompanyName:
      return u"Company";
    case aa::MemoryDataType::kIban:
      return u"IBAN";
    case aa::MemoryDataType::kIbanNickname:
      return u"Name";
    case aa::MemoryDataType::kCreditCardNumber:
      return u"Card number";
    case aa::MemoryDataType::kCreditCardExpirationDate:
      return u"Expiration date";
    case aa::MemoryDataType::kCreditCardSecurityCode:
      return u"Security code";
    case aa::MemoryDataType::kCreditCardNameOnCard:
      return u"Name on card";
    case aa::MemoryDataType::kCreditCardNickname:
      return u"Card Nickname";
    // Entity types:
    case aa::MemoryDataType::kVehicle:
    case aa::MemoryDataType::kPassportFull:
    case aa::MemoryDataType::kFlightReservationFull:
    case aa::MemoryDataType::kNationalIdCardFull:
    case aa::MemoryDataType::kRedressNumberFull:
    case aa::MemoryDataType::kKnownTravelerNumberFull:
    case aa::MemoryDataType::kDriversLicenseFull:
    case aa::MemoryDataType::kOrderFull:
    case aa::MemoryDataType::kShipmentFull: {
      std::optional<AtMemoryDataType> data_type = ToAtMemoryDataType(type);
      const auto* entity_type =
          data_type ? std::get_if<EntityType>(&*data_type) : nullptr;
      return entity_type ? entity_type->GetNameForI18n() : u"";
    }
    // Attribute types:
    case aa::MemoryDataType::kVehicleMake:
    case aa::MemoryDataType::kVehicleModel:
    case aa::MemoryDataType::kVehicleYear:
    case aa::MemoryDataType::kVehicleOwner:
    case aa::MemoryDataType::kVehiclePlateNumber:
    case aa::MemoryDataType::kVehiclePlateState:
    case aa::MemoryDataType::kVehicleVin:
    case aa::MemoryDataType::kPassportName:
    case aa::MemoryDataType::kPassportCountry:
    case aa::MemoryDataType::kPassportNumber:
    case aa::MemoryDataType::kPassportIssueDate:
    case aa::MemoryDataType::kPassportExpirationDate:
    case aa::MemoryDataType::kFlightReservationFlightNumber:
    case aa::MemoryDataType::kFlightReservationTicketNumber:
    case aa::MemoryDataType::kFlightReservationConfirmationCode:
    case aa::MemoryDataType::kFlightReservationPassengerName:
    case aa::MemoryDataType::kFlightReservationDepartureAirport:
    case aa::MemoryDataType::kFlightReservationArrivalAirport:
    case aa::MemoryDataType::kFlightReservationDepartureDate:
    case aa::MemoryDataType::kFlightReservationArrivalDate:
    case aa::MemoryDataType::kNationalIdCardName:
    case aa::MemoryDataType::kNationalIdCardCountry:
    case aa::MemoryDataType::kNationalIdCardNumber:
    case aa::MemoryDataType::kNationalIdCardIssueDate:
    case aa::MemoryDataType::kNationalIdCardExpirationDate:
    case aa::MemoryDataType::kRedressNumberName:
    case aa::MemoryDataType::kRedressNumberNumber:
    case aa::MemoryDataType::kKnownTravelerNumberName:
    case aa::MemoryDataType::kKnownTravelerNumberNumber:
    case aa::MemoryDataType::kKnownTravelerNumberExpirationDate:
    case aa::MemoryDataType::kDriversLicenseName:
    case aa::MemoryDataType::kDriversLicenseState:
    case aa::MemoryDataType::kDriversLicenseNumber:
    case aa::MemoryDataType::kDriversLicenseIssueDate:
    case aa::MemoryDataType::kDriversLicenseExpirationDate:
    case aa::MemoryDataType::kOrderId:
    case aa::MemoryDataType::kOrderAccount:
    case aa::MemoryDataType::kOrderDate:
    case aa::MemoryDataType::kOrderMerchantName:
    case aa::MemoryDataType::kOrderMerchantDomain:
    case aa::MemoryDataType::kOrderProductNames:
    case aa::MemoryDataType::kOrderGrandTotal:
    case aa::MemoryDataType::kShipmentTrackingNumber:
    case aa::MemoryDataType::kShipmentAssociatedOrderId:
    case aa::MemoryDataType::kShipmentDeliveryAddress:
    case aa::MemoryDataType::kShipmentDeliveryZipCode:
    case aa::MemoryDataType::kShipmentCarrierName:
    case aa::MemoryDataType::kShipmentCarrierDomain:
    case aa::MemoryDataType::kShipmentEstimatedDeliveryDate:
    case aa::MemoryDataType::kShipmentShippedDate: {
      std::optional<AtMemoryDataType> data_type = ToAtMemoryDataType(type);
      const auto* attribute_type =
          data_type ? std::get_if<AttributeType>(&*data_type) : nullptr;
      return attribute_type ? attribute_type->GetNameForI18n() : u"";
    }
  }
}

}  // namespace autofill
