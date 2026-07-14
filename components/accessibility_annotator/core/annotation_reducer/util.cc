// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/util.h"

#include <optional>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"

namespace accessibility_annotator {

std::string_view StripMarkdownCodeBlocks(std::string_view text) {
  if (base::StartsWith(text, "```json", base::CompareCase::SENSITIVE)) {
    text.remove_prefix(7);
  } else if (base::StartsWith(text, "```", base::CompareCase::SENSITIVE)) {
    text.remove_prefix(3);
  }

  if (base::EndsWith(text, "```", base::CompareCase::SENSITIVE)) {
    text.remove_suffix(3);
  }

  return base::TrimWhitespaceASCII(text, base::TRIM_ALL);
}

// TODO(crbug.com/496281633): Avoid using a custom string to enum conversion.
MemoryDataType StringToMemoryDataType(std::string_view intent_str) {
  // LINT.IfChange(MemoryDataType)
  static constexpr auto kIntentMap = base::MakeFixedFlatMap<std::string_view,
                                                            MemoryDataType>({
      {"kAddressCity", MemoryDataType::kAddressCity},
      {"kAddressCountry", MemoryDataType::kAddressCountry},
      {"kAddressFull", MemoryDataType::kAddressFull},
      {"kAddressState", MemoryDataType::kAddressState},
      {"kAddressStreetAddress", MemoryDataType::kAddressStreetAddress},
      {"kAddressZip", MemoryDataType::kAddressZip},
      {"kCompanyName", MemoryDataType::kCompanyName},
      {"kCreditCardExpirationDate", MemoryDataType::kCreditCardExpirationDate},
      {"kCreditCardNameOnCard", MemoryDataType::kCreditCardNameOnCard},
      {"kCreditCardNickname", MemoryDataType::kCreditCardNickname},
      {"kCreditCardNumber", MemoryDataType::kCreditCardNumber},
      {"kCreditCardSecurityCode", MemoryDataType::kCreditCardSecurityCode},
      {"kDriversLicenseExpirationDate",
       MemoryDataType::kDriversLicenseExpirationDate},
      {"kDriversLicenseFull", MemoryDataType::kDriversLicenseFull},
      {"kDriversLicenseIssueDate", MemoryDataType::kDriversLicenseIssueDate},
      {"kDriversLicenseName", MemoryDataType::kDriversLicenseName},
      {"kDriversLicenseNumber", MemoryDataType::kDriversLicenseNumber},
      {"kDriversLicenseState", MemoryDataType::kDriversLicenseState},
      {"kEmail", MemoryDataType::kEmail},
      {"kFlightReservationArrivalAirport",
       MemoryDataType::kFlightReservationArrivalAirport},
      {"kFlightReservationConfirmationCode",
       MemoryDataType::kFlightReservationConfirmationCode},
      {"kFlightReservationDepartureAirport",
       MemoryDataType::kFlightReservationDepartureAirport},
      {"kFlightReservationDepartureDate",
       MemoryDataType::kFlightReservationDepartureDate},
      {"kFlightReservationFlightNumber",
       MemoryDataType::kFlightReservationFlightNumber},
      {"kFlightReservationFull", MemoryDataType::kFlightReservationFull},
      {"kFlightReservationPassengerName",
       MemoryDataType::kFlightReservationPassengerName},
      {"kFlightReservationTicketNumber",
       MemoryDataType::kFlightReservationTicketNumber},
      {"kIban", MemoryDataType::kIban},
      {"kIbanNickname", MemoryDataType::kIbanNickname},
      {"kKnownTravelerNumberExpirationDate",
       MemoryDataType::kKnownTravelerNumberExpirationDate},
      {"kKnownTravelerNumberFull", MemoryDataType::kKnownTravelerNumberFull},
      {"kKnownTravelerNumberName", MemoryDataType::kKnownTravelerNumberName},
      {"kKnownTravelerNumberNumber",
       MemoryDataType::kKnownTravelerNumberNumber},
      {"kNameFull", MemoryDataType::kNameFull},
      {"kNationalIdCardCountry", MemoryDataType::kNationalIdCardCountry},
      {"kNationalIdCardExpirationDate",
       MemoryDataType::kNationalIdCardExpirationDate},
      {"kNationalIdCardFull", MemoryDataType::kNationalIdCardFull},
      {"kNationalIdCardIssueDate", MemoryDataType::kNationalIdCardIssueDate},
      {"kNationalIdCardName", MemoryDataType::kNationalIdCardName},
      {"kNationalIdCardNumber", MemoryDataType::kNationalIdCardNumber},
      {"kOrderAccount", MemoryDataType::kOrderAccount},
      {"kOrderDate", MemoryDataType::kOrderDate},
      {"kOrderFull", MemoryDataType::kOrderFull},
      {"kOrderGrandTotal", MemoryDataType::kOrderGrandTotal},
      {"kOrderId", MemoryDataType::kOrderId},
      {"kOrderMerchantDomain", MemoryDataType::kOrderMerchantDomain},
      {"kOrderMerchantName", MemoryDataType::kOrderMerchantName},
      {"kOrderProductNames", MemoryDataType::kOrderProductNames},
      {"kPassportCountry", MemoryDataType::kPassportCountry},
      {"kPassportExpirationDate", MemoryDataType::kPassportExpirationDate},
      {"kPassportFull", MemoryDataType::kPassportFull},
      {"kPassportIssueDate", MemoryDataType::kPassportIssueDate},
      {"kPassportName", MemoryDataType::kPassportName},
      {"kPassportNumber", MemoryDataType::kPassportNumber},
      {"kPhone", MemoryDataType::kPhone},
      {"kRedressNumberFull", MemoryDataType::kRedressNumberFull},
      {"kRedressNumberName", MemoryDataType::kRedressNumberName},
      {"kRedressNumberNumber", MemoryDataType::kRedressNumberNumber},
      {"kShipmentShippedDate", MemoryDataType::kShipmentShippedDate},
      {"kVehicle", MemoryDataType::kVehicle},
      {"kVehicleMake", MemoryDataType::kVehicleMake},
      {"kVehicleModel", MemoryDataType::kVehicleModel},
      {"kVehicleOwner", MemoryDataType::kVehicleOwner},
      {"kVehiclePlateNumber", MemoryDataType::kVehiclePlateNumber},
      {"kVehiclePlateState", MemoryDataType::kVehiclePlateState},
      {"kVehicleVin", MemoryDataType::kVehicleVin},
      {"kVehicleYear", MemoryDataType::kVehicleYear},
  });
  // LINT.ThenChange(//components/accessibility_annotator/core/annotation_reducer/memory_data_type.h:MemoryDataType)

  auto found_intent_it = kIntentMap.find(intent_str);
  return found_intent_it != kIntentMap.end() ? found_intent_it->second
                                             : MemoryDataType::kUnknown;
}

// Note: The set of MemoryDataType can be a superset of AnswerType. However,
// deletion of a MemoryDataType that is currently mapped to an AnswerType
// requires server-side changes.
// LINT.IfChange(AnswerTypeToMemoryDataType)
std::optional<MemoryDataType> AnswerTypeToMemoryDataType(
    optimization_guide::proto::ReducedAnswer::AnswerType answer_type) {
  switch (answer_type) {
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_UNKNOWN:
      return MemoryDataType::kUnknown;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_NAME_FULL:
      return MemoryDataType::kNameFull;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ADDRESS_FULL:
      return MemoryDataType::kAddressFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_ADDRESS_STREET_ADDRESS:
      return MemoryDataType::kAddressStreetAddress;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ADDRESS_CITY:
      return MemoryDataType::kAddressCity;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ADDRESS_STATE:
      return MemoryDataType::kAddressState;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ADDRESS_ZIP:
      return MemoryDataType::kAddressZip;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ADDRESS_COUNTRY:
      return MemoryDataType::kAddressCountry;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_PHONE:
      return MemoryDataType::kPhone;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_EMAIL:
      return MemoryDataType::kEmail;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_COMPANY_NAME:
      return MemoryDataType::kCompanyName;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_IBAN:
      return MemoryDataType::kIban;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_IBAN_NICKNAME:
      return MemoryDataType::kIbanNickname;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_VEHICLE:
      return MemoryDataType::kVehicle;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_VEHICLE_MAKE:
      return MemoryDataType::kVehicleMake;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_VEHICLE_MODEL:
      return MemoryDataType::kVehicleModel;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_VEHICLE_YEAR:
      return MemoryDataType::kVehicleYear;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_VEHICLE_OWNER:
      return MemoryDataType::kVehicleOwner;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_VEHICLE_PLATE_NUMBER:
      return MemoryDataType::kVehiclePlateNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_VEHICLE_PLATE_STATE:
      return MemoryDataType::kVehiclePlateState;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_VEHICLE_VIN:
      return MemoryDataType::kVehicleVin;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_PASSPORT_FULL:
      return MemoryDataType::kPassportFull;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_PASSPORT_NAME:
      return MemoryDataType::kPassportName;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_PASSPORT_COUNTRY:
      return MemoryDataType::kPassportCountry;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_PASSPORT_NUMBER:
      return MemoryDataType::kPassportNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_PASSPORT_ISSUE_DATE:
      return MemoryDataType::kPassportIssueDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_PASSPORT_EXPIRATION_DATE:
      return MemoryDataType::kPassportExpirationDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_FULL:
      return MemoryDataType::kFlightReservationFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_FLIGHT_NUMBER:
      return MemoryDataType::kFlightReservationFlightNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_TICKET_NUMBER:
      return MemoryDataType::kFlightReservationTicketNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_CONFIRMATION_CODE:
      return MemoryDataType::kFlightReservationConfirmationCode;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_PASSENGER_NAME:
      return MemoryDataType::kFlightReservationPassengerName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_DEPARTURE_AIRPORT:
      return MemoryDataType::kFlightReservationDepartureAirport;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_ARRIVAL_AIRPORT:
      return MemoryDataType::kFlightReservationArrivalAirport;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_DEPARTURE_DATE:
      return MemoryDataType::kFlightReservationDepartureDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_ARRIVAL_DATE:
      return MemoryDataType::kFlightReservationArrivalDate;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_SHIPMENT_FULL:
      return MemoryDataType::kShipmentFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_SHIPMENT_TRACKING_NUMBER:
      return MemoryDataType::kShipmentTrackingNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_SHIPMENT_ASSOCIATED_ORDER_ID:
      return MemoryDataType::kShipmentAssociatedOrderId;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_SHIPMENT_DELIVERY_ADDRESS:
      return MemoryDataType::kShipmentDeliveryAddress;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_SHIPMENT_CARRIER_NAME:
      return MemoryDataType::kShipmentCarrierName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_SHIPMENT_CARRIER_DOMAIN:
      return MemoryDataType::kShipmentCarrierDomain;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_SHIPMENT_ESTIMATED_DELIVERY_DATE:
      return MemoryDataType::kShipmentEstimatedDeliveryDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_NATIONAL_ID_CARD_FULL:
      return MemoryDataType::kNationalIdCardFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_NATIONAL_ID_CARD_NAME:
      return MemoryDataType::kNationalIdCardName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_NATIONAL_ID_CARD_COUNTRY:
      return MemoryDataType::kNationalIdCardCountry;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_NATIONAL_ID_CARD_NUMBER:
      return MemoryDataType::kNationalIdCardNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_NATIONAL_ID_CARD_ISSUE_DATE:
      return MemoryDataType::kNationalIdCardIssueDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_NATIONAL_ID_CARD_EXPIRATION_DATE:
      return MemoryDataType::kNationalIdCardExpirationDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_REDRESS_NUMBER_FULL:
      return MemoryDataType::kRedressNumberFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_REDRESS_NUMBER_NAME:
      return MemoryDataType::kRedressNumberName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_REDRESS_NUMBER_NUMBER:
      return MemoryDataType::kRedressNumberNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_FULL:
      return MemoryDataType::kKnownTravelerNumberFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_NAME:
      return MemoryDataType::kKnownTravelerNumberName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_NUMBER:
      return MemoryDataType::kKnownTravelerNumberNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE:
      return MemoryDataType::kKnownTravelerNumberExpirationDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_DRIVERS_LICENSE_FULL:
      return MemoryDataType::kDriversLicenseFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_DRIVERS_LICENSE_NAME:
      return MemoryDataType::kDriversLicenseName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_DRIVERS_LICENSE_STATE:
      return MemoryDataType::kDriversLicenseState;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_DRIVERS_LICENSE_NUMBER:
      return MemoryDataType::kDriversLicenseNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_DRIVERS_LICENSE_ISSUE_DATE:
      return MemoryDataType::kDriversLicenseIssueDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_DRIVERS_LICENSE_EXPIRATION_DATE:
      return MemoryDataType::kDriversLicenseExpirationDate;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ORDER_FULL:
      return MemoryDataType::kOrderFull;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ORDER_ID:
      return MemoryDataType::kOrderId;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ORDER_ACCOUNT:
      return MemoryDataType::kOrderAccount;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ORDER_DATE:
      return MemoryDataType::kOrderDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_ORDER_MERCHANT_NAME:
      return MemoryDataType::kOrderMerchantName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_ORDER_MERCHANT_DOMAIN:
      return MemoryDataType::kOrderMerchantDomain;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_ORDER_PRODUCT_NAMES:
      return MemoryDataType::kOrderProductNames;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_ORDER_GRAND_TOTAL:
      return MemoryDataType::kOrderGrandTotal;
    default:
      return std::nullopt;
  }
}
// LINT.ThenChange(//components/accessibility_annotator/core/annotation_reducer/memory_data_type.h:MemoryDataType)

// LINT.IfChange(SourceTypeToMemoryEntrySourceType)
std::optional<MemoryEntrySourceType> SourceTypeToMemoryEntrySourceType(
    optimization_guide::proto::ReducedAnswer::Source::SourceType type) {
  switch (type) {
    case optimization_guide::proto::ReducedAnswer::Source::SOURCE_TYPE_AUTOFILL:
      return MemoryEntrySourceType::kAutofill;
    case optimization_guide::proto::ReducedAnswer::Source::SOURCE_TYPE_GMAIL:
      return MemoryEntrySourceType::kGmail;
    case optimization_guide::proto::ReducedAnswer::Source::SOURCE_TYPE_CALENDAR:
      return MemoryEntrySourceType::kCalendar;
    case optimization_guide::proto::ReducedAnswer::Source::SOURCE_TYPE_PHOTOS:
      return MemoryEntrySourceType::kPhotos;
    default:
      return std::nullopt;
  }
}
// LINT.ThenChange(//components/accessibility_annotator/core/annotation_reducer/memory_search_result.h:MemoryEntrySourceType)

}  // namespace accessibility_annotator
