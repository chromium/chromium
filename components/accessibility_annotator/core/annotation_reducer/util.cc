// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/util.h"

#include <optional>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"

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
QueryIntentType StringToQueryIntentType(std::string_view intent_str) {
  // LINT.IfChange(QueryIntentType)
  static constexpr auto kIntentMap = base::MakeFixedFlatMap<std::string_view,
                                                            QueryIntentType>({
      {"kAddressCity", QueryIntentType::kAddressCity},
      {"kAddressCountry", QueryIntentType::kAddressCountry},
      {"kAddressFull", QueryIntentType::kAddressFull},
      {"kAddressState", QueryIntentType::kAddressState},
      {"kAddressStreetAddress", QueryIntentType::kAddressStreetAddress},
      {"kAddressZip", QueryIntentType::kAddressZip},
      {"kCompanyName", QueryIntentType::kCompanyName},
      {"kCreditCardExpirationDate", QueryIntentType::kCreditCardExpirationDate},
      {"kCreditCardFull", QueryIntentType::kCreditCardFull},
      {"kCreditCardNameOnCard", QueryIntentType::kCreditCardNameOnCard},
      {"kCreditCardNickname", QueryIntentType::kCreditCardNickname},
      {"kCreditCardNumber", QueryIntentType::kCreditCardNumber},
      {"kCreditCardSecurityCode", QueryIntentType::kCreditCardSecurityCode},
      {"kDriversLicenseExpirationDate",
       QueryIntentType::kDriversLicenseExpirationDate},
      {"kDriversLicenseFull", QueryIntentType::kDriversLicenseFull},
      {"kDriversLicenseIssueDate", QueryIntentType::kDriversLicenseIssueDate},
      {"kDriversLicenseName", QueryIntentType::kDriversLicenseName},
      {"kDriversLicenseNumber", QueryIntentType::kDriversLicenseNumber},
      {"kDriversLicenseState", QueryIntentType::kDriversLicenseState},
      {"kEmail", QueryIntentType::kEmail},
      {"kFlightReservationArrivalAirport",
       QueryIntentType::kFlightReservationArrivalAirport},
      {"kFlightReservationConfirmationCode",
       QueryIntentType::kFlightReservationConfirmationCode},
      {"kFlightReservationDepartureAirport",
       QueryIntentType::kFlightReservationDepartureAirport},
      {"kFlightReservationDepartureDate",
       QueryIntentType::kFlightReservationDepartureDate},
      {"kFlightReservationFlightNumber",
       QueryIntentType::kFlightReservationFlightNumber},
      {"kFlightReservationFull", QueryIntentType::kFlightReservationFull},
      {"kFlightReservationPassengerName",
       QueryIntentType::kFlightReservationPassengerName},
      {"kFlightReservationTicketNumber",
       QueryIntentType::kFlightReservationTicketNumber},
      {"kIban", QueryIntentType::kIban},
      {"kIbanNickname", QueryIntentType::kIbanNickname},
      {"kKnownTravelerNumberExpirationDate",
       QueryIntentType::kKnownTravelerNumberExpirationDate},
      {"kKnownTravelerNumberFull", QueryIntentType::kKnownTravelerNumberFull},
      {"kKnownTravelerNumberName", QueryIntentType::kKnownTravelerNumberName},
      {"kKnownTravelerNumberNumber",
       QueryIntentType::kKnownTravelerNumberNumber},
      {"kNameFull", QueryIntentType::kNameFull},
      {"kNationalIdCardCountry", QueryIntentType::kNationalIdCardCountry},
      {"kNationalIdCardExpirationDate",
       QueryIntentType::kNationalIdCardExpirationDate},
      {"kNationalIdCardFull", QueryIntentType::kNationalIdCardFull},
      {"kNationalIdCardIssueDate", QueryIntentType::kNationalIdCardIssueDate},
      {"kNationalIdCardName", QueryIntentType::kNationalIdCardName},
      {"kNationalIdCardNumber", QueryIntentType::kNationalIdCardNumber},
      {"kOrderAccount", QueryIntentType::kOrderAccount},
      {"kOrderDate", QueryIntentType::kOrderDate},
      {"kOrderFull", QueryIntentType::kOrderFull},
      {"kOrderGrandTotal", QueryIntentType::kOrderGrandTotal},
      {"kOrderId", QueryIntentType::kOrderId},
      {"kOrderMerchantDomain", QueryIntentType::kOrderMerchantDomain},
      {"kOrderMerchantName", QueryIntentType::kOrderMerchantName},
      {"kOrderProductNames", QueryIntentType::kOrderProductNames},
      {"kPassportCountry", QueryIntentType::kPassportCountry},
      {"kPassportExpirationDate", QueryIntentType::kPassportExpirationDate},
      {"kPassportFull", QueryIntentType::kPassportFull},
      {"kPassportIssueDate", QueryIntentType::kPassportIssueDate},
      {"kPassportName", QueryIntentType::kPassportName},
      {"kPassportNumber", QueryIntentType::kPassportNumber},
      {"kPhone", QueryIntentType::kPhone},
      {"kRedressNumberFull", QueryIntentType::kRedressNumberFull},
      {"kRedressNumberName", QueryIntentType::kRedressNumberName},
      {"kRedressNumberNumber", QueryIntentType::kRedressNumberNumber},
      {"kVehicle", QueryIntentType::kVehicle},
      {"kVehicleMake", QueryIntentType::kVehicleMake},
      {"kVehicleModel", QueryIntentType::kVehicleModel},
      {"kVehicleOwner", QueryIntentType::kVehicleOwner},
      {"kVehiclePlateNumber", QueryIntentType::kVehiclePlateNumber},
      {"kVehiclePlateState", QueryIntentType::kVehiclePlateState},
      {"kVehicleVin", QueryIntentType::kVehicleVin},
      {"kVehicleYear", QueryIntentType::kVehicleYear},
  });
  // LINT.ThenChange(//components/accessibility_annotator/core/annotation_reducer/query_intent_type.h:QueryIntentType)

  auto found_intent_it = kIntentMap.find(intent_str);
  return found_intent_it != kIntentMap.end() ? found_intent_it->second
                                             : QueryIntentType::kUnknown;
}

// Note: The set of QueryIntentType can be a superset of AnswerType. However,
// deletion of a QueryIntentType that is currently mapped to an AnswerType
// requires server-side changes.
// LINT.IfChange(AnswerTypeToQueryIntentType)
std::optional<QueryIntentType> AnswerTypeToQueryIntentType(
    optimization_guide::proto::ReducedAnswer::AnswerType answer_type) {
  switch (answer_type) {
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_UNKNOWN:
      return QueryIntentType::kUnknown;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_NAME_FULL:
      return QueryIntentType::kNameFull;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ADDRESS_FULL:
      return QueryIntentType::kAddressFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_ADDRESS_STREET_ADDRESS:
      return QueryIntentType::kAddressStreetAddress;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ADDRESS_CITY:
      return QueryIntentType::kAddressCity;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ADDRESS_STATE:
      return QueryIntentType::kAddressState;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ADDRESS_ZIP:
      return QueryIntentType::kAddressZip;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ADDRESS_COUNTRY:
      return QueryIntentType::kAddressCountry;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_PHONE:
      return QueryIntentType::kPhone;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_EMAIL:
      return QueryIntentType::kEmail;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_COMPANY_NAME:
      return QueryIntentType::kCompanyName;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_IBAN:
      return QueryIntentType::kIban;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_IBAN_NICKNAME:
      return QueryIntentType::kIbanNickname;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_VEHICLE:
      return QueryIntentType::kVehicle;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_VEHICLE_MAKE:
      return QueryIntentType::kVehicleMake;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_VEHICLE_MODEL:
      return QueryIntentType::kVehicleModel;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_VEHICLE_YEAR:
      return QueryIntentType::kVehicleYear;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_VEHICLE_OWNER:
      return QueryIntentType::kVehicleOwner;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_VEHICLE_PLATE_NUMBER:
      return QueryIntentType::kVehiclePlateNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_VEHICLE_PLATE_STATE:
      return QueryIntentType::kVehiclePlateState;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_VEHICLE_VIN:
      return QueryIntentType::kVehicleVin;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_PASSPORT_FULL:
      return QueryIntentType::kPassportFull;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_PASSPORT_NAME:
      return QueryIntentType::kPassportName;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_PASSPORT_COUNTRY:
      return QueryIntentType::kPassportCountry;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_PASSPORT_NUMBER:
      return QueryIntentType::kPassportNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_PASSPORT_ISSUE_DATE:
      return QueryIntentType::kPassportIssueDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_PASSPORT_EXPIRATION_DATE:
      return QueryIntentType::kPassportExpirationDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_FULL:
      return QueryIntentType::kFlightReservationFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_FLIGHT_NUMBER:
      return QueryIntentType::kFlightReservationFlightNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_TICKET_NUMBER:
      return QueryIntentType::kFlightReservationTicketNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_CONFIRMATION_CODE:
      return QueryIntentType::kFlightReservationConfirmationCode;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_PASSENGER_NAME:
      return QueryIntentType::kFlightReservationPassengerName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_DEPARTURE_AIRPORT:
      return QueryIntentType::kFlightReservationDepartureAirport;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_ARRIVAL_AIRPORT:
      return QueryIntentType::kFlightReservationArrivalAirport;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_DEPARTURE_DATE:
      return QueryIntentType::kFlightReservationDepartureDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_ARRIVAL_DATE:
      return QueryIntentType::kFlightReservationArrivalDate;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_SHIPMENT_FULL:
      return QueryIntentType::kShipmentFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_SHIPMENT_TRACKING_NUMBER:
      return QueryIntentType::kShipmentTrackingNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_SHIPMENT_ASSOCIATED_ORDER_ID:
      return QueryIntentType::kShipmentAssociatedOrderId;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_SHIPMENT_DELIVERY_ADDRESS:
      return QueryIntentType::kShipmentDeliveryAddress;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_SHIPMENT_CARRIER_NAME:
      return QueryIntentType::kShipmentCarrierName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_SHIPMENT_CARRIER_DOMAIN:
      return QueryIntentType::kShipmentCarrierDomain;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_SHIPMENT_ESTIMATED_DELIVERY_DATE:
      return QueryIntentType::kShipmentEstimatedDeliveryDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_NATIONAL_ID_CARD_FULL:
      return QueryIntentType::kNationalIdCardFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_NATIONAL_ID_CARD_NAME:
      return QueryIntentType::kNationalIdCardName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_NATIONAL_ID_CARD_COUNTRY:
      return QueryIntentType::kNationalIdCardCountry;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_NATIONAL_ID_CARD_NUMBER:
      return QueryIntentType::kNationalIdCardNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_NATIONAL_ID_CARD_ISSUE_DATE:
      return QueryIntentType::kNationalIdCardIssueDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_NATIONAL_ID_CARD_EXPIRATION_DATE:
      return QueryIntentType::kNationalIdCardExpirationDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_REDRESS_NUMBER_FULL:
      return QueryIntentType::kRedressNumberFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_REDRESS_NUMBER_NAME:
      return QueryIntentType::kRedressNumberName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_REDRESS_NUMBER_NUMBER:
      return QueryIntentType::kRedressNumberNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_FULL:
      return QueryIntentType::kKnownTravelerNumberFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_NAME:
      return QueryIntentType::kKnownTravelerNumberName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_NUMBER:
      return QueryIntentType::kKnownTravelerNumberNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE:
      return QueryIntentType::kKnownTravelerNumberExpirationDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_DRIVERS_LICENSE_FULL:
      return QueryIntentType::kDriversLicenseFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_DRIVERS_LICENSE_NAME:
      return QueryIntentType::kDriversLicenseName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_DRIVERS_LICENSE_STATE:
      return QueryIntentType::kDriversLicenseState;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_DRIVERS_LICENSE_NUMBER:
      return QueryIntentType::kDriversLicenseNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_DRIVERS_LICENSE_ISSUE_DATE:
      return QueryIntentType::kDriversLicenseIssueDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_DRIVERS_LICENSE_EXPIRATION_DATE:
      return QueryIntentType::kDriversLicenseExpirationDate;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ORDER_FULL:
      return QueryIntentType::kOrderFull;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ORDER_ID:
      return QueryIntentType::kOrderId;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ORDER_ACCOUNT:
      return QueryIntentType::kOrderAccount;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ORDER_DATE:
      return QueryIntentType::kOrderDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_ORDER_MERCHANT_NAME:
      return QueryIntentType::kOrderMerchantName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_ORDER_MERCHANT_DOMAIN:
      return QueryIntentType::kOrderMerchantDomain;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_ORDER_PRODUCT_NAMES:
      return QueryIntentType::kOrderProductNames;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_ORDER_GRAND_TOTAL:
      return QueryIntentType::kOrderGrandTotal;
    default:
      return std::nullopt;
  }
}
// LINT.ThenChange(//components/accessibility_annotator/core/annotation_reducer/query_intent_type.h:QueryIntentType)

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
    case optimization_guide::proto::ReducedAnswer::Source::SOURCE_TYPE_AMBIENT:
      return MemoryEntrySourceType::kAmbient;
    case optimization_guide::proto::ReducedAnswer::Source::
        SOURCE_TYPE_LIVE_TABS:
      return MemoryEntrySourceType::kLiveTabs;
    default:
      return std::nullopt;
  }
}
// LINT.ThenChange(//components/accessibility_annotator/core/annotation_reducer/memory_search_result.h:MemoryEntrySourceType)

}  // namespace accessibility_annotator
