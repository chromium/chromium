// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/util.h"

#include <optional>

#include "components/accessibility_annotator/core/annotation_reducer/entry_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/optimization_guide/proto/features/annotation_reducer_one_p_resolver.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {
namespace {

using ReducedAnswer = optimization_guide::proto::ReducedAnswer;

// Verifies that valid markdown code blocks are correctly stripped from strings.
TEST(AnnotationReducerUtilTest, StripMarkdownCodeBlocks_ValidCodeBlocks) {
  EXPECT_EQ(StripMarkdownCodeBlocks("```json\n{ \"a\": 1 }\n```"),
            "{ \"a\": 1 }");
  EXPECT_EQ(StripMarkdownCodeBlocks("```\nhello\n```"), "hello");
  EXPECT_EQ(StripMarkdownCodeBlocks("```json { \"a\": 1 } ```"),
            "{ \"a\": 1 }");
}

// Verifies that leading and trailing whitespaces are correctly trimmed.
TEST(AnnotationReducerUtilTest, StripMarkdownCodeBlocks_WhitespaceTrimming) {
  EXPECT_EQ(StripMarkdownCodeBlocks("hello"), "hello");
  EXPECT_EQ(StripMarkdownCodeBlocks("   hello   "), "hello");
}

// Verifies that empty strings and incomplete markdown blocks are handled
// safely.
TEST(AnnotationReducerUtilTest, StripMarkdownCodeBlocks_EmptyAndInvalid) {
  EXPECT_EQ(StripMarkdownCodeBlocks("```json\n"), "");
  EXPECT_EQ(StripMarkdownCodeBlocks("```"), "");
  EXPECT_EQ(StripMarkdownCodeBlocks(""), "");
}

// Verifies that valid string intents are correctly mapped to their
// corresponding EntryType enums.
TEST(AnnotationReducerUtilTest, StringToEntryType_ValidMappings) {
  EXPECT_EQ(StringToEntryType("kAddressCity"), EntryType::kAddressCity);
  EXPECT_EQ(StringToEntryType("kAddressCountry"), EntryType::kAddressCountry);
  EXPECT_EQ(StringToEntryType("kAddressFull"), EntryType::kAddressFull);
  EXPECT_EQ(StringToEntryType("kAddressState"), EntryType::kAddressState);
  EXPECT_EQ(StringToEntryType("kAddressStreetAddress"),
            EntryType::kAddressStreetAddress);
  EXPECT_EQ(StringToEntryType("kAddressZip"), EntryType::kAddressZip);
  EXPECT_EQ(StringToEntryType("kCompanyName"), EntryType::kCompanyName);
  EXPECT_EQ(StringToEntryType("kCreditCardExpirationDate"),
            EntryType::kCreditCardExpirationDate);
  EXPECT_EQ(StringToEntryType("kCreditCardNameOnCard"),
            EntryType::kCreditCardNameOnCard);
  EXPECT_EQ(StringToEntryType("kCreditCardNickname"),
            EntryType::kCreditCardNickname);
  EXPECT_EQ(StringToEntryType("kCreditCardNumber"),
            EntryType::kCreditCardNumber);
  EXPECT_EQ(StringToEntryType("kCreditCardSecurityCode"),
            EntryType::kCreditCardSecurityCode);
  EXPECT_EQ(StringToEntryType("kDriversLicenseExpirationDate"),
            EntryType::kDriversLicenseExpirationDate);
  EXPECT_EQ(StringToEntryType("kDriversLicenseFull"),
            EntryType::kDriversLicenseFull);
  EXPECT_EQ(StringToEntryType("kDriversLicenseIssueDate"),
            EntryType::kDriversLicenseIssueDate);
  EXPECT_EQ(StringToEntryType("kDriversLicenseName"),
            EntryType::kDriversLicenseName);
  EXPECT_EQ(StringToEntryType("kDriversLicenseNumber"),
            EntryType::kDriversLicenseNumber);
  EXPECT_EQ(StringToEntryType("kDriversLicenseState"),
            EntryType::kDriversLicenseState);
  EXPECT_EQ(StringToEntryType("kEmail"), EntryType::kEmail);
  EXPECT_EQ(StringToEntryType("kFlightReservationArrivalAirport"),
            EntryType::kFlightReservationArrivalAirport);
  EXPECT_EQ(StringToEntryType("kFlightReservationConfirmationCode"),
            EntryType::kFlightReservationConfirmationCode);
  EXPECT_EQ(StringToEntryType("kFlightReservationDepartureAirport"),
            EntryType::kFlightReservationDepartureAirport);
  EXPECT_EQ(StringToEntryType("kFlightReservationDepartureDate"),
            EntryType::kFlightReservationDepartureDate);
  EXPECT_EQ(StringToEntryType("kFlightReservationFlightNumber"),
            EntryType::kFlightReservationFlightNumber);
  EXPECT_EQ(StringToEntryType("kFlightReservationFull"),
            EntryType::kFlightReservationFull);
  EXPECT_EQ(StringToEntryType("kFlightReservationPassengerName"),
            EntryType::kFlightReservationPassengerName);
  EXPECT_EQ(StringToEntryType("kFlightReservationTicketNumber"),
            EntryType::kFlightReservationTicketNumber);
  EXPECT_EQ(StringToEntryType("kIban"), EntryType::kIban);
  EXPECT_EQ(StringToEntryType("kIbanNickname"), EntryType::kIbanNickname);
  EXPECT_EQ(StringToEntryType("kKnownTravelerNumberExpirationDate"),
            EntryType::kKnownTravelerNumberExpirationDate);
  EXPECT_EQ(StringToEntryType("kKnownTravelerNumberFull"),
            EntryType::kKnownTravelerNumberFull);
  EXPECT_EQ(StringToEntryType("kKnownTravelerNumberName"),
            EntryType::kKnownTravelerNumberName);
  EXPECT_EQ(StringToEntryType("kKnownTravelerNumberNumber"),
            EntryType::kKnownTravelerNumberNumber);
  EXPECT_EQ(StringToEntryType("kNameFull"), EntryType::kNameFull);
  EXPECT_EQ(StringToEntryType("kNationalIdCardCountry"),
            EntryType::kNationalIdCardCountry);
  EXPECT_EQ(StringToEntryType("kNationalIdCardExpirationDate"),
            EntryType::kNationalIdCardExpirationDate);
  EXPECT_EQ(StringToEntryType("kNationalIdCardFull"),
            EntryType::kNationalIdCardFull);
  EXPECT_EQ(StringToEntryType("kNationalIdCardIssueDate"),
            EntryType::kNationalIdCardIssueDate);
  EXPECT_EQ(StringToEntryType("kNationalIdCardName"),
            EntryType::kNationalIdCardName);
  EXPECT_EQ(StringToEntryType("kNationalIdCardNumber"),
            EntryType::kNationalIdCardNumber);
  EXPECT_EQ(StringToEntryType("kOrderAccount"), EntryType::kOrderAccount);
  EXPECT_EQ(StringToEntryType("kOrderDate"), EntryType::kOrderDate);
  EXPECT_EQ(StringToEntryType("kOrderFull"), EntryType::kOrderFull);
  EXPECT_EQ(StringToEntryType("kOrderGrandTotal"), EntryType::kOrderGrandTotal);
  EXPECT_EQ(StringToEntryType("kOrderId"), EntryType::kOrderId);
  EXPECT_EQ(StringToEntryType("kOrderMerchantDomain"),
            EntryType::kOrderMerchantDomain);
  EXPECT_EQ(StringToEntryType("kOrderMerchantName"),
            EntryType::kOrderMerchantName);
  EXPECT_EQ(StringToEntryType("kOrderProductNames"),
            EntryType::kOrderProductNames);
  EXPECT_EQ(StringToEntryType("kPassportCountry"), EntryType::kPassportCountry);
  EXPECT_EQ(StringToEntryType("kPassportExpirationDate"),
            EntryType::kPassportExpirationDate);
  EXPECT_EQ(StringToEntryType("kPassportFull"), EntryType::kPassportFull);
  EXPECT_EQ(StringToEntryType("kPassportIssueDate"),
            EntryType::kPassportIssueDate);
  EXPECT_EQ(StringToEntryType("kPassportName"), EntryType::kPassportName);
  EXPECT_EQ(StringToEntryType("kPassportNumber"), EntryType::kPassportNumber);
  EXPECT_EQ(StringToEntryType("kPhone"), EntryType::kPhone);
  EXPECT_EQ(StringToEntryType("kRedressNumberFull"),
            EntryType::kRedressNumberFull);
  EXPECT_EQ(StringToEntryType("kRedressNumberName"),
            EntryType::kRedressNumberName);
  EXPECT_EQ(StringToEntryType("kRedressNumberNumber"),
            EntryType::kRedressNumberNumber);
  EXPECT_EQ(StringToEntryType("kVehicle"), EntryType::kVehicle);
  EXPECT_EQ(StringToEntryType("kVehicleMake"), EntryType::kVehicleMake);
  EXPECT_EQ(StringToEntryType("kVehicleModel"), EntryType::kVehicleModel);
  EXPECT_EQ(StringToEntryType("kVehicleOwner"), EntryType::kVehicleOwner);
  EXPECT_EQ(StringToEntryType("kVehiclePlateNumber"),
            EntryType::kVehiclePlateNumber);
  EXPECT_EQ(StringToEntryType("kVehiclePlateState"),
            EntryType::kVehiclePlateState);
  EXPECT_EQ(StringToEntryType("kVehicleVin"), EntryType::kVehicleVin);
  EXPECT_EQ(StringToEntryType("kVehicleYear"), EntryType::kVehicleYear);
}

// Verifies that an unrecognized or empty string intent is safely mapped to
// kUnknown.
TEST(AnnotationReducerUtilTest, StringToEntryType_InvalidMapping) {
  EXPECT_EQ(StringToEntryType("kNonExistentIntent"), EntryType::kUnknown);
  EXPECT_EQ(StringToEntryType(""), EntryType::kUnknown);
}

// Verifies that all valid AnswerType proto enums correctly map to
// EntryType enums.
TEST(AnnotationReducerUtilTest, AnswerTypeToEntryType_ValidMappings) {
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_UNKNOWN),
            EntryType::kUnknown);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_NAME_FULL),
            EntryType::kNameFull);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_ADDRESS_FULL),
            EntryType::kAddressFull);
  EXPECT_EQ(
      AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_ADDRESS_STREET_ADDRESS),
      EntryType::kAddressStreetAddress);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_ADDRESS_CITY),
            EntryType::kAddressCity);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_ADDRESS_STATE),
            EntryType::kAddressState);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_ADDRESS_ZIP),
            EntryType::kAddressZip);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_ADDRESS_COUNTRY),
            EntryType::kAddressCountry);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_PHONE),
            EntryType::kPhone);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_EMAIL),
            EntryType::kEmail);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_COMPANY_NAME),
            EntryType::kCompanyName);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_IBAN),
            EntryType::kIban);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_IBAN_NICKNAME),
            EntryType::kIbanNickname);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_VEHICLE),
            EntryType::kVehicle);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_VEHICLE_MAKE),
            EntryType::kVehicleMake);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_VEHICLE_MODEL),
            EntryType::kVehicleModel);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_VEHICLE_YEAR),
            EntryType::kVehicleYear);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_VEHICLE_OWNER),
            EntryType::kVehicleOwner);
  EXPECT_EQ(
      AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_VEHICLE_PLATE_NUMBER),
      EntryType::kVehiclePlateNumber);
  EXPECT_EQ(
      AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_VEHICLE_PLATE_STATE),
      EntryType::kVehiclePlateState);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_VEHICLE_VIN),
            EntryType::kVehicleVin);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_PASSPORT_FULL),
            EntryType::kPassportFull);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_PASSPORT_NAME),
            EntryType::kPassportName);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_PASSPORT_COUNTRY),
            EntryType::kPassportCountry);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_PASSPORT_NUMBER),
            EntryType::kPassportNumber);
  EXPECT_EQ(
      AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_PASSPORT_ISSUE_DATE),
      EntryType::kPassportIssueDate);
  EXPECT_EQ(AnswerTypeToEntryType(
                ReducedAnswer::ANSWER_TYPE_PASSPORT_EXPIRATION_DATE),
            EntryType::kPassportExpirationDate);
  EXPECT_EQ(
      AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_FULL),
      EntryType::kFlightReservationFull);
  EXPECT_EQ(AnswerTypeToEntryType(
                ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_FLIGHT_NUMBER),
            EntryType::kFlightReservationFlightNumber);
  EXPECT_EQ(AnswerTypeToEntryType(
                ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_TICKET_NUMBER),
            EntryType::kFlightReservationTicketNumber);
  EXPECT_EQ(
      AnswerTypeToEntryType(
          ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_CONFIRMATION_CODE),
      EntryType::kFlightReservationConfirmationCode);
  EXPECT_EQ(AnswerTypeToEntryType(
                ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_PASSENGER_NAME),
            EntryType::kFlightReservationPassengerName);
  EXPECT_EQ(
      AnswerTypeToEntryType(
          ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_DEPARTURE_AIRPORT),
      EntryType::kFlightReservationDepartureAirport);
  EXPECT_EQ(AnswerTypeToEntryType(
                ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_ARRIVAL_AIRPORT),
            EntryType::kFlightReservationArrivalAirport);
  EXPECT_EQ(AnswerTypeToEntryType(
                ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_DEPARTURE_DATE),
            EntryType::kFlightReservationDepartureDate);
  EXPECT_EQ(AnswerTypeToEntryType(
                ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_ARRIVAL_DATE),
            EntryType::kFlightReservationArrivalDate);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_SHIPMENT_FULL),
            EntryType::kShipmentFull);
  EXPECT_EQ(AnswerTypeToEntryType(
                ReducedAnswer::ANSWER_TYPE_SHIPMENT_TRACKING_NUMBER),
            EntryType::kShipmentTrackingNumber);
  EXPECT_EQ(AnswerTypeToEntryType(
                ReducedAnswer::ANSWER_TYPE_SHIPMENT_ASSOCIATED_ORDER_ID),
            EntryType::kShipmentAssociatedOrderId);
  EXPECT_EQ(AnswerTypeToEntryType(
                ReducedAnswer::ANSWER_TYPE_SHIPMENT_DELIVERY_ADDRESS),
            EntryType::kShipmentDeliveryAddress);
  EXPECT_EQ(
      AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_SHIPMENT_CARRIER_NAME),
      EntryType::kShipmentCarrierName);
  EXPECT_EQ(
      AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_SHIPMENT_CARRIER_DOMAIN),
      EntryType::kShipmentCarrierDomain);
  EXPECT_EQ(AnswerTypeToEntryType(
                ReducedAnswer::ANSWER_TYPE_SHIPMENT_ESTIMATED_DELIVERY_DATE),
            EntryType::kShipmentEstimatedDeliveryDate);
  EXPECT_EQ(
      AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_NATIONAL_ID_CARD_FULL),
      EntryType::kNationalIdCardFull);
  EXPECT_EQ(
      AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_NATIONAL_ID_CARD_NAME),
      EntryType::kNationalIdCardName);
  EXPECT_EQ(AnswerTypeToEntryType(
                ReducedAnswer::ANSWER_TYPE_NATIONAL_ID_CARD_COUNTRY),
            EntryType::kNationalIdCardCountry);
  EXPECT_EQ(
      AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_NATIONAL_ID_CARD_NUMBER),
      EntryType::kNationalIdCardNumber);
  EXPECT_EQ(AnswerTypeToEntryType(
                ReducedAnswer::ANSWER_TYPE_NATIONAL_ID_CARD_ISSUE_DATE),
            EntryType::kNationalIdCardIssueDate);
  EXPECT_EQ(AnswerTypeToEntryType(
                ReducedAnswer::ANSWER_TYPE_NATIONAL_ID_CARD_EXPIRATION_DATE),
            EntryType::kNationalIdCardExpirationDate);
  EXPECT_EQ(
      AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_REDRESS_NUMBER_FULL),
      EntryType::kRedressNumberFull);
  EXPECT_EQ(
      AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_REDRESS_NUMBER_NAME),
      EntryType::kRedressNumberName);
  EXPECT_EQ(
      AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_REDRESS_NUMBER_NUMBER),
      EntryType::kRedressNumberNumber);
  EXPECT_EQ(AnswerTypeToEntryType(
                ReducedAnswer::ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_FULL),
            EntryType::kKnownTravelerNumberFull);
  EXPECT_EQ(AnswerTypeToEntryType(
                ReducedAnswer::ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_NAME),
            EntryType::kKnownTravelerNumberName);
  EXPECT_EQ(AnswerTypeToEntryType(
                ReducedAnswer::ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_NUMBER),
            EntryType::kKnownTravelerNumberNumber);
  EXPECT_EQ(
      AnswerTypeToEntryType(
          ReducedAnswer::ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE),
      EntryType::kKnownTravelerNumberExpirationDate);
  EXPECT_EQ(
      AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_DRIVERS_LICENSE_FULL),
      EntryType::kDriversLicenseFull);
  EXPECT_EQ(
      AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_DRIVERS_LICENSE_NAME),
      EntryType::kDriversLicenseName);
  EXPECT_EQ(
      AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_DRIVERS_LICENSE_STATE),
      EntryType::kDriversLicenseState);
  EXPECT_EQ(
      AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_DRIVERS_LICENSE_NUMBER),
      EntryType::kDriversLicenseNumber);
  EXPECT_EQ(AnswerTypeToEntryType(
                ReducedAnswer::ANSWER_TYPE_DRIVERS_LICENSE_ISSUE_DATE),
            EntryType::kDriversLicenseIssueDate);
  EXPECT_EQ(AnswerTypeToEntryType(
                ReducedAnswer::ANSWER_TYPE_DRIVERS_LICENSE_EXPIRATION_DATE),
            EntryType::kDriversLicenseExpirationDate);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_ORDER_FULL),
            EntryType::kOrderFull);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_ORDER_ID),
            EntryType::kOrderId);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_ORDER_ACCOUNT),
            EntryType::kOrderAccount);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_ORDER_DATE),
            EntryType::kOrderDate);
  EXPECT_EQ(
      AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_ORDER_MERCHANT_NAME),
      EntryType::kOrderMerchantName);
  EXPECT_EQ(
      AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_ORDER_MERCHANT_DOMAIN),
      EntryType::kOrderMerchantDomain);
  EXPECT_EQ(
      AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_ORDER_PRODUCT_NAMES),
      EntryType::kOrderProductNames);
  EXPECT_EQ(AnswerTypeToEntryType(ReducedAnswer::ANSWER_TYPE_ORDER_GRAND_TOTAL),
            EntryType::kOrderGrandTotal);
}

// Verifies that an out-of-bounds AnswerType proto enum maps to std::nullopt.
TEST(AnnotationReducerUtilTest, AnswerTypeToEntryType_InvalidMapping) {
  EXPECT_EQ(AnswerTypeToEntryType(static_cast<ReducedAnswer::AnswerType>(9999)),
            std::nullopt);
}

// Verifies that all valid SourceType proto enums correctly map to
// MemoryEntrySourceType enums.
TEST(AnnotationReducerUtilTest,
     SourceTypeToMemoryEntrySourceType_ValidMappings) {
  EXPECT_EQ(SourceTypeToMemoryEntrySourceType(
                ReducedAnswer::Source::SOURCE_TYPE_AUTOFILL),
            MemoryEntrySourceType::kAutofill);
  EXPECT_EQ(SourceTypeToMemoryEntrySourceType(
                ReducedAnswer::Source::SOURCE_TYPE_GMAIL),
            MemoryEntrySourceType::kGmail);
  EXPECT_EQ(SourceTypeToMemoryEntrySourceType(
                ReducedAnswer::Source::SOURCE_TYPE_CALENDAR),
            MemoryEntrySourceType::kCalendar);
  EXPECT_EQ(SourceTypeToMemoryEntrySourceType(
                ReducedAnswer::Source::SOURCE_TYPE_PHOTOS),
            MemoryEntrySourceType::kPhotos);
  EXPECT_EQ(SourceTypeToMemoryEntrySourceType(
                ReducedAnswer::Source::SOURCE_TYPE_AMBIENT),
            MemoryEntrySourceType::kAmbient);
  EXPECT_EQ(SourceTypeToMemoryEntrySourceType(
                ReducedAnswer::Source::SOURCE_TYPE_LIVE_TABS),
            MemoryEntrySourceType::kLiveTabs);
}

// Verifies that an out-of-bounds SourceType proto enum maps to std::nullopt.
TEST(AnnotationReducerUtilTest,
     SourceTypeToMemoryEntrySourceType_InvalidMapping) {
  EXPECT_EQ(SourceTypeToMemoryEntrySourceType(
                static_cast<ReducedAnswer::Source::SourceType>(9999)),
            std::nullopt);
}

}  // namespace
}  // namespace accessibility_annotator
