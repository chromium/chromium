// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/util.h"

#include <optional>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "components/accessibility_annotator/core/annotation_reducer/entry_type.h"
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
EntryType StringToEntryType(std::string_view intent_str) {
  // LINT.IfChange(EntryType)
  static constexpr auto kIntentMap =
      base::MakeFixedFlatMap<std::string_view, EntryType>({
          {"kAddressCity", EntryType::kAddressCity},
          {"kAddressCountry", EntryType::kAddressCountry},
          {"kAddressFull", EntryType::kAddressFull},
          {"kAddressState", EntryType::kAddressState},
          {"kAddressStreetAddress", EntryType::kAddressStreetAddress},
          {"kAddressZip", EntryType::kAddressZip},
          {"kCompanyName", EntryType::kCompanyName},
          {"kCreditCardExpirationDate", EntryType::kCreditCardExpirationDate},
          {"kCreditCardNameOnCard", EntryType::kCreditCardNameOnCard},
          {"kCreditCardNickname", EntryType::kCreditCardNickname},
          {"kCreditCardNumber", EntryType::kCreditCardNumber},
          {"kCreditCardSecurityCode", EntryType::kCreditCardSecurityCode},
          {"kDriversLicenseExpirationDate",
           EntryType::kDriversLicenseExpirationDate},
          {"kDriversLicenseFull", EntryType::kDriversLicenseFull},
          {"kDriversLicenseIssueDate", EntryType::kDriversLicenseIssueDate},
          {"kDriversLicenseName", EntryType::kDriversLicenseName},
          {"kDriversLicenseNumber", EntryType::kDriversLicenseNumber},
          {"kDriversLicenseState", EntryType::kDriversLicenseState},
          {"kEmail", EntryType::kEmail},
          {"kFlightReservationArrivalAirport",
           EntryType::kFlightReservationArrivalAirport},
          {"kFlightReservationConfirmationCode",
           EntryType::kFlightReservationConfirmationCode},
          {"kFlightReservationDepartureAirport",
           EntryType::kFlightReservationDepartureAirport},
          {"kFlightReservationDepartureDate",
           EntryType::kFlightReservationDepartureDate},
          {"kFlightReservationFlightNumber",
           EntryType::kFlightReservationFlightNumber},
          {"kFlightReservationFull", EntryType::kFlightReservationFull},
          {"kFlightReservationPassengerName",
           EntryType::kFlightReservationPassengerName},
          {"kFlightReservationTicketNumber",
           EntryType::kFlightReservationTicketNumber},
          {"kIban", EntryType::kIban},
          {"kIbanNickname", EntryType::kIbanNickname},
          {"kKnownTravelerNumberExpirationDate",
           EntryType::kKnownTravelerNumberExpirationDate},
          {"kKnownTravelerNumberFull", EntryType::kKnownTravelerNumberFull},
          {"kKnownTravelerNumberName", EntryType::kKnownTravelerNumberName},
          {"kKnownTravelerNumberNumber", EntryType::kKnownTravelerNumberNumber},
          {"kNameFull", EntryType::kNameFull},
          {"kNationalIdCardCountry", EntryType::kNationalIdCardCountry},
          {"kNationalIdCardExpirationDate",
           EntryType::kNationalIdCardExpirationDate},
          {"kNationalIdCardFull", EntryType::kNationalIdCardFull},
          {"kNationalIdCardIssueDate", EntryType::kNationalIdCardIssueDate},
          {"kNationalIdCardName", EntryType::kNationalIdCardName},
          {"kNationalIdCardNumber", EntryType::kNationalIdCardNumber},
          {"kOrderAccount", EntryType::kOrderAccount},
          {"kOrderDate", EntryType::kOrderDate},
          {"kOrderFull", EntryType::kOrderFull},
          {"kOrderGrandTotal", EntryType::kOrderGrandTotal},
          {"kOrderId", EntryType::kOrderId},
          {"kOrderMerchantDomain", EntryType::kOrderMerchantDomain},
          {"kOrderMerchantName", EntryType::kOrderMerchantName},
          {"kOrderProductNames", EntryType::kOrderProductNames},
          {"kPassportCountry", EntryType::kPassportCountry},
          {"kPassportExpirationDate", EntryType::kPassportExpirationDate},
          {"kPassportFull", EntryType::kPassportFull},
          {"kPassportIssueDate", EntryType::kPassportIssueDate},
          {"kPassportName", EntryType::kPassportName},
          {"kPassportNumber", EntryType::kPassportNumber},
          {"kPhone", EntryType::kPhone},
          {"kRedressNumberFull", EntryType::kRedressNumberFull},
          {"kRedressNumberName", EntryType::kRedressNumberName},
          {"kRedressNumberNumber", EntryType::kRedressNumberNumber},
          {"kVehicle", EntryType::kVehicle},
          {"kVehicleMake", EntryType::kVehicleMake},
          {"kVehicleModel", EntryType::kVehicleModel},
          {"kVehicleOwner", EntryType::kVehicleOwner},
          {"kVehiclePlateNumber", EntryType::kVehiclePlateNumber},
          {"kVehiclePlateState", EntryType::kVehiclePlateState},
          {"kVehicleVin", EntryType::kVehicleVin},
          {"kVehicleYear", EntryType::kVehicleYear},
      });
  // LINT.ThenChange(//components/accessibility_annotator/core/annotation_reducer/entry_type.h:EntryType)

  auto found_intent_it = kIntentMap.find(intent_str);
  return found_intent_it != kIntentMap.end() ? found_intent_it->second
                                             : EntryType::kUnknown;
}

// Note: The set of EntryType can be a superset of AnswerType. However,
// deletion of a EntryType that is currently mapped to an AnswerType
// requires server-side changes.
// LINT.IfChange(AnswerTypeToEntryType)
std::optional<EntryType> AnswerTypeToEntryType(
    optimization_guide::proto::ReducedAnswer::AnswerType answer_type) {
  switch (answer_type) {
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_UNKNOWN:
      return EntryType::kUnknown;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_NAME_FULL:
      return EntryType::kNameFull;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ADDRESS_FULL:
      return EntryType::kAddressFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_ADDRESS_STREET_ADDRESS:
      return EntryType::kAddressStreetAddress;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ADDRESS_CITY:
      return EntryType::kAddressCity;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ADDRESS_STATE:
      return EntryType::kAddressState;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ADDRESS_ZIP:
      return EntryType::kAddressZip;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ADDRESS_COUNTRY:
      return EntryType::kAddressCountry;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_PHONE:
      return EntryType::kPhone;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_EMAIL:
      return EntryType::kEmail;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_COMPANY_NAME:
      return EntryType::kCompanyName;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_IBAN:
      return EntryType::kIban;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_IBAN_NICKNAME:
      return EntryType::kIbanNickname;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_VEHICLE:
      return EntryType::kVehicle;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_VEHICLE_MAKE:
      return EntryType::kVehicleMake;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_VEHICLE_MODEL:
      return EntryType::kVehicleModel;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_VEHICLE_YEAR:
      return EntryType::kVehicleYear;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_VEHICLE_OWNER:
      return EntryType::kVehicleOwner;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_VEHICLE_PLATE_NUMBER:
      return EntryType::kVehiclePlateNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_VEHICLE_PLATE_STATE:
      return EntryType::kVehiclePlateState;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_VEHICLE_VIN:
      return EntryType::kVehicleVin;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_PASSPORT_FULL:
      return EntryType::kPassportFull;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_PASSPORT_NAME:
      return EntryType::kPassportName;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_PASSPORT_COUNTRY:
      return EntryType::kPassportCountry;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_PASSPORT_NUMBER:
      return EntryType::kPassportNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_PASSPORT_ISSUE_DATE:
      return EntryType::kPassportIssueDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_PASSPORT_EXPIRATION_DATE:
      return EntryType::kPassportExpirationDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_FULL:
      return EntryType::kFlightReservationFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_FLIGHT_NUMBER:
      return EntryType::kFlightReservationFlightNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_TICKET_NUMBER:
      return EntryType::kFlightReservationTicketNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_CONFIRMATION_CODE:
      return EntryType::kFlightReservationConfirmationCode;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_PASSENGER_NAME:
      return EntryType::kFlightReservationPassengerName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_DEPARTURE_AIRPORT:
      return EntryType::kFlightReservationDepartureAirport;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_ARRIVAL_AIRPORT:
      return EntryType::kFlightReservationArrivalAirport;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_DEPARTURE_DATE:
      return EntryType::kFlightReservationDepartureDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_FLIGHT_RESERVATION_ARRIVAL_DATE:
      return EntryType::kFlightReservationArrivalDate;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_SHIPMENT_FULL:
      return EntryType::kShipmentFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_SHIPMENT_TRACKING_NUMBER:
      return EntryType::kShipmentTrackingNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_SHIPMENT_ASSOCIATED_ORDER_ID:
      return EntryType::kShipmentAssociatedOrderId;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_SHIPMENT_DELIVERY_ADDRESS:
      return EntryType::kShipmentDeliveryAddress;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_SHIPMENT_CARRIER_NAME:
      return EntryType::kShipmentCarrierName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_SHIPMENT_CARRIER_DOMAIN:
      return EntryType::kShipmentCarrierDomain;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_SHIPMENT_ESTIMATED_DELIVERY_DATE:
      return EntryType::kShipmentEstimatedDeliveryDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_NATIONAL_ID_CARD_FULL:
      return EntryType::kNationalIdCardFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_NATIONAL_ID_CARD_NAME:
      return EntryType::kNationalIdCardName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_NATIONAL_ID_CARD_COUNTRY:
      return EntryType::kNationalIdCardCountry;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_NATIONAL_ID_CARD_NUMBER:
      return EntryType::kNationalIdCardNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_NATIONAL_ID_CARD_ISSUE_DATE:
      return EntryType::kNationalIdCardIssueDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_NATIONAL_ID_CARD_EXPIRATION_DATE:
      return EntryType::kNationalIdCardExpirationDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_REDRESS_NUMBER_FULL:
      return EntryType::kRedressNumberFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_REDRESS_NUMBER_NAME:
      return EntryType::kRedressNumberName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_REDRESS_NUMBER_NUMBER:
      return EntryType::kRedressNumberNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_FULL:
      return EntryType::kKnownTravelerNumberFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_NAME:
      return EntryType::kKnownTravelerNumberName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_NUMBER:
      return EntryType::kKnownTravelerNumberNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE:
      return EntryType::kKnownTravelerNumberExpirationDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_DRIVERS_LICENSE_FULL:
      return EntryType::kDriversLicenseFull;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_DRIVERS_LICENSE_NAME:
      return EntryType::kDriversLicenseName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_DRIVERS_LICENSE_STATE:
      return EntryType::kDriversLicenseState;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_DRIVERS_LICENSE_NUMBER:
      return EntryType::kDriversLicenseNumber;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_DRIVERS_LICENSE_ISSUE_DATE:
      return EntryType::kDriversLicenseIssueDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_DRIVERS_LICENSE_EXPIRATION_DATE:
      return EntryType::kDriversLicenseExpirationDate;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ORDER_FULL:
      return EntryType::kOrderFull;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ORDER_ID:
      return EntryType::kOrderId;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ORDER_ACCOUNT:
      return EntryType::kOrderAccount;
    case optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_ORDER_DATE:
      return EntryType::kOrderDate;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_ORDER_MERCHANT_NAME:
      return EntryType::kOrderMerchantName;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_ORDER_MERCHANT_DOMAIN:
      return EntryType::kOrderMerchantDomain;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_ORDER_PRODUCT_NAMES:
      return EntryType::kOrderProductNames;
    case optimization_guide::proto::ReducedAnswer::
        ANSWER_TYPE_ORDER_GRAND_TOTAL:
      return EntryType::kOrderGrandTotal;
    default:
      return std::nullopt;
  }
}
// LINT.ThenChange(//components/accessibility_annotator/core/annotation_reducer/entry_type.h:EntryType)

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
