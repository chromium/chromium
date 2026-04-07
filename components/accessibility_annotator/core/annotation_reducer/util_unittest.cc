// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/util.h"

#include <optional>

#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"
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
// corresponding QueryIntentType enums.
TEST(AnnotationReducerUtilTest, StringToQueryIntentType_ValidMappings) {
  EXPECT_EQ(StringToQueryIntentType("kAddressCity"),
            QueryIntentType::kAddressCity);
  EXPECT_EQ(StringToQueryIntentType("kAddressCountry"),
            QueryIntentType::kAddressCountry);
  EXPECT_EQ(StringToQueryIntentType("kAddressFull"),
            QueryIntentType::kAddressFull);
  EXPECT_EQ(StringToQueryIntentType("kAddressState"),
            QueryIntentType::kAddressState);
  EXPECT_EQ(StringToQueryIntentType("kAddressStreetAddress"),
            QueryIntentType::kAddressStreetAddress);
  EXPECT_EQ(StringToQueryIntentType("kAddressZip"),
            QueryIntentType::kAddressZip);
  EXPECT_EQ(StringToQueryIntentType("kCompanyName"),
            QueryIntentType::kCompanyName);
  EXPECT_EQ(StringToQueryIntentType("kCreditCardExpirationDate"),
            QueryIntentType::kCreditCardExpirationDate);
  EXPECT_EQ(StringToQueryIntentType("kCreditCardFull"),
            QueryIntentType::kCreditCardFull);
  EXPECT_EQ(StringToQueryIntentType("kCreditCardNameOnCard"),
            QueryIntentType::kCreditCardNameOnCard);
  EXPECT_EQ(StringToQueryIntentType("kCreditCardNickname"),
            QueryIntentType::kCreditCardNickname);
  EXPECT_EQ(StringToQueryIntentType("kCreditCardNumber"),
            QueryIntentType::kCreditCardNumber);
  EXPECT_EQ(StringToQueryIntentType("kCreditCardSecurityCode"),
            QueryIntentType::kCreditCardSecurityCode);
  EXPECT_EQ(StringToQueryIntentType("kDriversLicenseExpirationDate"),
            QueryIntentType::kDriversLicenseExpirationDate);
  EXPECT_EQ(StringToQueryIntentType("kDriversLicenseFull"),
            QueryIntentType::kDriversLicenseFull);
  EXPECT_EQ(StringToQueryIntentType("kDriversLicenseIssueDate"),
            QueryIntentType::kDriversLicenseIssueDate);
  EXPECT_EQ(StringToQueryIntentType("kDriversLicenseName"),
            QueryIntentType::kDriversLicenseName);
  EXPECT_EQ(StringToQueryIntentType("kDriversLicenseNumber"),
            QueryIntentType::kDriversLicenseNumber);
  EXPECT_EQ(StringToQueryIntentType("kDriversLicenseState"),
            QueryIntentType::kDriversLicenseState);
  EXPECT_EQ(StringToQueryIntentType("kEmail"), QueryIntentType::kEmail);
  EXPECT_EQ(StringToQueryIntentType("kFlightReservationArrivalAirport"),
            QueryIntentType::kFlightReservationArrivalAirport);
  EXPECT_EQ(StringToQueryIntentType("kFlightReservationConfirmationCode"),
            QueryIntentType::kFlightReservationConfirmationCode);
  EXPECT_EQ(StringToQueryIntentType("kFlightReservationDepartureAirport"),
            QueryIntentType::kFlightReservationDepartureAirport);
  EXPECT_EQ(StringToQueryIntentType("kFlightReservationDepartureDate"),
            QueryIntentType::kFlightReservationDepartureDate);
  EXPECT_EQ(StringToQueryIntentType("kFlightReservationFlightNumber"),
            QueryIntentType::kFlightReservationFlightNumber);
  EXPECT_EQ(StringToQueryIntentType("kFlightReservationFull"),
            QueryIntentType::kFlightReservationFull);
  EXPECT_EQ(StringToQueryIntentType("kFlightReservationPassengerName"),
            QueryIntentType::kFlightReservationPassengerName);
  EXPECT_EQ(StringToQueryIntentType("kFlightReservationTicketNumber"),
            QueryIntentType::kFlightReservationTicketNumber);
  EXPECT_EQ(StringToQueryIntentType("kIban"), QueryIntentType::kIban);
  EXPECT_EQ(StringToQueryIntentType("kIbanNickname"),
            QueryIntentType::kIbanNickname);
  EXPECT_EQ(StringToQueryIntentType("kKnownTravelerNumberExpirationDate"),
            QueryIntentType::kKnownTravelerNumberExpirationDate);
  EXPECT_EQ(StringToQueryIntentType("kKnownTravelerNumberFull"),
            QueryIntentType::kKnownTravelerNumberFull);
  EXPECT_EQ(StringToQueryIntentType("kKnownTravelerNumberName"),
            QueryIntentType::kKnownTravelerNumberName);
  EXPECT_EQ(StringToQueryIntentType("kKnownTravelerNumberNumber"),
            QueryIntentType::kKnownTravelerNumberNumber);
  EXPECT_EQ(StringToQueryIntentType("kNameFull"), QueryIntentType::kNameFull);
  EXPECT_EQ(StringToQueryIntentType("kNationalIdCardCountry"),
            QueryIntentType::kNationalIdCardCountry);
  EXPECT_EQ(StringToQueryIntentType("kNationalIdCardExpirationDate"),
            QueryIntentType::kNationalIdCardExpirationDate);
  EXPECT_EQ(StringToQueryIntentType("kNationalIdCardFull"),
            QueryIntentType::kNationalIdCardFull);
  EXPECT_EQ(StringToQueryIntentType("kNationalIdCardIssueDate"),
            QueryIntentType::kNationalIdCardIssueDate);
  EXPECT_EQ(StringToQueryIntentType("kNationalIdCardName"),
            QueryIntentType::kNationalIdCardName);
  EXPECT_EQ(StringToQueryIntentType("kNationalIdCardNumber"),
            QueryIntentType::kNationalIdCardNumber);
  EXPECT_EQ(StringToQueryIntentType("kOrderAccount"),
            QueryIntentType::kOrderAccount);
  EXPECT_EQ(StringToQueryIntentType("kOrderDate"), QueryIntentType::kOrderDate);
  EXPECT_EQ(StringToQueryIntentType("kOrderFull"), QueryIntentType::kOrderFull);
  EXPECT_EQ(StringToQueryIntentType("kOrderGrandTotal"),
            QueryIntentType::kOrderGrandTotal);
  EXPECT_EQ(StringToQueryIntentType("kOrderId"), QueryIntentType::kOrderId);
  EXPECT_EQ(StringToQueryIntentType("kOrderMerchantDomain"),
            QueryIntentType::kOrderMerchantDomain);
  EXPECT_EQ(StringToQueryIntentType("kOrderMerchantName"),
            QueryIntentType::kOrderMerchantName);
  EXPECT_EQ(StringToQueryIntentType("kOrderProductNames"),
            QueryIntentType::kOrderProductNames);
  EXPECT_EQ(StringToQueryIntentType("kPassportCountry"),
            QueryIntentType::kPassportCountry);
  EXPECT_EQ(StringToQueryIntentType("kPassportExpirationDate"),
            QueryIntentType::kPassportExpirationDate);
  EXPECT_EQ(StringToQueryIntentType("kPassportFull"),
            QueryIntentType::kPassportFull);
  EXPECT_EQ(StringToQueryIntentType("kPassportIssueDate"),
            QueryIntentType::kPassportIssueDate);
  EXPECT_EQ(StringToQueryIntentType("kPassportName"),
            QueryIntentType::kPassportName);
  EXPECT_EQ(StringToQueryIntentType("kPassportNumber"),
            QueryIntentType::kPassportNumber);
  EXPECT_EQ(StringToQueryIntentType("kPhone"), QueryIntentType::kPhone);
  EXPECT_EQ(StringToQueryIntentType("kRedressNumberFull"),
            QueryIntentType::kRedressNumberFull);
  EXPECT_EQ(StringToQueryIntentType("kRedressNumberName"),
            QueryIntentType::kRedressNumberName);
  EXPECT_EQ(StringToQueryIntentType("kRedressNumberNumber"),
            QueryIntentType::kRedressNumberNumber);
  EXPECT_EQ(StringToQueryIntentType("kVehicle"), QueryIntentType::kVehicle);
  EXPECT_EQ(StringToQueryIntentType("kVehicleMake"),
            QueryIntentType::kVehicleMake);
  EXPECT_EQ(StringToQueryIntentType("kVehicleModel"),
            QueryIntentType::kVehicleModel);
  EXPECT_EQ(StringToQueryIntentType("kVehicleOwner"),
            QueryIntentType::kVehicleOwner);
  EXPECT_EQ(StringToQueryIntentType("kVehiclePlateNumber"),
            QueryIntentType::kVehiclePlateNumber);
  EXPECT_EQ(StringToQueryIntentType("kVehiclePlateState"),
            QueryIntentType::kVehiclePlateState);
  EXPECT_EQ(StringToQueryIntentType("kVehicleVin"),
            QueryIntentType::kVehicleVin);
  EXPECT_EQ(StringToQueryIntentType("kVehicleYear"),
            QueryIntentType::kVehicleYear);
}

// Verifies that an unrecognized or empty string intent is safely mapped to
// kUnknown.
TEST(AnnotationReducerUtilTest, StringToQueryIntentType_InvalidMapping) {
  EXPECT_EQ(StringToQueryIntentType("kNonExistentIntent"),
            QueryIntentType::kUnknown);
  EXPECT_EQ(StringToQueryIntentType(""), QueryIntentType::kUnknown);
}

// Verifies that all valid AnswerType proto enums correctly map to
// QueryIntentType enums.
TEST(AnnotationReducerUtilTest, AnswerTypeToQueryIntentType_ValidMappings) {
  EXPECT_EQ(AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_UNKNOWN),
            QueryIntentType::kUnknown);
  EXPECT_EQ(AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_NAME_FULL),
            QueryIntentType::kNameFull);
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_ADDRESS_FULL),
      QueryIntentType::kAddressFull);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_ADDRESS_STREET_ADDRESS),
            QueryIntentType::kAddressStreetAddress);
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_ADDRESS_CITY),
      QueryIntentType::kAddressCity);
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_ADDRESS_STATE),
      QueryIntentType::kAddressState);
  EXPECT_EQ(AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_ADDRESS_ZIP),
            QueryIntentType::kAddressZip);
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_ADDRESS_COUNTRY),
      QueryIntentType::kAddressCountry);
  EXPECT_EQ(AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_PHONE),
            QueryIntentType::kPhone);
  EXPECT_EQ(AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_EMAIL),
            QueryIntentType::kEmail);
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_COMPANY_NAME),
      QueryIntentType::kCompanyName);
  EXPECT_EQ(AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_IBAN),
            QueryIntentType::kIban);
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_IBAN_NICKNAME),
      QueryIntentType::kIbanNickname);
  EXPECT_EQ(AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_VEHICLE),
            QueryIntentType::kVehicle);
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_VEHICLE_MAKE),
      QueryIntentType::kVehicleMake);
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_VEHICLE_MODEL),
      QueryIntentType::kVehicleModel);
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_VEHICLE_YEAR),
      QueryIntentType::kVehicleYear);
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_VEHICLE_OWNER),
      QueryIntentType::kVehicleOwner);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_VEHICLE_PLATE_NUMBER),
            QueryIntentType::kVehiclePlateNumber);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_VEHICLE_PLATE_STATE),
            QueryIntentType::kVehiclePlateState);
  EXPECT_EQ(AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_VEHICLE_VIN),
            QueryIntentType::kVehicleVin);
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_PASSPORT_FULL),
      QueryIntentType::kPassportFull);
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_PASSPORT_NAME),
      QueryIntentType::kPassportName);
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_PASSPORT_COUNTRY),
      QueryIntentType::kPassportCountry);
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_PASSPORT_NUMBER),
      QueryIntentType::kPassportNumber);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_PASSPORT_ISSUE_DATE),
            QueryIntentType::kPassportIssueDate);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_PASSPORT_EXPIRATION_DATE),
            QueryIntentType::kPassportExpirationDate);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_FULL),
            QueryIntentType::kFlightReservationFull);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_FLIGHT_NUMBER),
            QueryIntentType::kFlightReservationFlightNumber);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_TICKET_NUMBER),
            QueryIntentType::kFlightReservationTicketNumber);
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(
          ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_CONFIRMATION_CODE),
      QueryIntentType::kFlightReservationConfirmationCode);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_PASSENGER_NAME),
            QueryIntentType::kFlightReservationPassengerName);
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(
          ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_DEPARTURE_AIRPORT),
      QueryIntentType::kFlightReservationDepartureAirport);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_ARRIVAL_AIRPORT),
            QueryIntentType::kFlightReservationArrivalAirport);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_DEPARTURE_DATE),
            QueryIntentType::kFlightReservationDepartureDate);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_ARRIVAL_DATE),
            QueryIntentType::kFlightReservationArrivalDate);
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_SHIPMENT_FULL),
      QueryIntentType::kShipmentFull);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_SHIPMENT_TRACKING_NUMBER),
            QueryIntentType::kShipmentTrackingNumber);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_SHIPMENT_ASSOCIATED_ORDER_ID),
            QueryIntentType::kShipmentAssociatedOrderId);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_SHIPMENT_DELIVERY_ADDRESS),
            QueryIntentType::kShipmentDeliveryAddress);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_SHIPMENT_CARRIER_NAME),
            QueryIntentType::kShipmentCarrierName);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_SHIPMENT_CARRIER_DOMAIN),
            QueryIntentType::kShipmentCarrierDomain);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_SHIPMENT_ESTIMATED_DELIVERY_DATE),
            QueryIntentType::kShipmentEstimatedDeliveryDate);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_NATIONAL_ID_CARD_FULL),
            QueryIntentType::kNationalIdCardFull);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_NATIONAL_ID_CARD_NAME),
            QueryIntentType::kNationalIdCardName);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_NATIONAL_ID_CARD_COUNTRY),
            QueryIntentType::kNationalIdCardCountry);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_NATIONAL_ID_CARD_NUMBER),
            QueryIntentType::kNationalIdCardNumber);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_NATIONAL_ID_CARD_ISSUE_DATE),
            QueryIntentType::kNationalIdCardIssueDate);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_NATIONAL_ID_CARD_EXPIRATION_DATE),
            QueryIntentType::kNationalIdCardExpirationDate);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_REDRESS_NUMBER_FULL),
            QueryIntentType::kRedressNumberFull);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_REDRESS_NUMBER_NAME),
            QueryIntentType::kRedressNumberName);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_REDRESS_NUMBER_NUMBER),
            QueryIntentType::kRedressNumberNumber);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_FULL),
            QueryIntentType::kKnownTravelerNumberFull);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_NAME),
            QueryIntentType::kKnownTravelerNumberName);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_NUMBER),
            QueryIntentType::kKnownTravelerNumberNumber);
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(
          ReducedAnswer::ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE),
      QueryIntentType::kKnownTravelerNumberExpirationDate);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_DRIVERS_LICENSE_FULL),
            QueryIntentType::kDriversLicenseFull);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_DRIVERS_LICENSE_NAME),
            QueryIntentType::kDriversLicenseName);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_DRIVERS_LICENSE_STATE),
            QueryIntentType::kDriversLicenseState);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_DRIVERS_LICENSE_NUMBER),
            QueryIntentType::kDriversLicenseNumber);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_DRIVERS_LICENSE_ISSUE_DATE),
            QueryIntentType::kDriversLicenseIssueDate);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_DRIVERS_LICENSE_EXPIRATION_DATE),
            QueryIntentType::kDriversLicenseExpirationDate);
  EXPECT_EQ(AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_ORDER_FULL),
            QueryIntentType::kOrderFull);
  EXPECT_EQ(AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_ORDER_ID),
            QueryIntentType::kOrderId);
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_ORDER_ACCOUNT),
      QueryIntentType::kOrderAccount);
  EXPECT_EQ(AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_ORDER_DATE),
            QueryIntentType::kOrderDate);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_ORDER_MERCHANT_NAME),
            QueryIntentType::kOrderMerchantName);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_ORDER_MERCHANT_DOMAIN),
            QueryIntentType::kOrderMerchantDomain);
  EXPECT_EQ(AnswerTypeToQueryIntentType(
                ReducedAnswer::ANSWER_TYPE_ORDER_PRODUCT_NAMES),
            QueryIntentType::kOrderProductNames);
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(ReducedAnswer::ANSWER_TYPE_ORDER_GRAND_TOTAL),
      QueryIntentType::kOrderGrandTotal);
}

// Verifies that an out-of-bounds AnswerType proto enum maps to std::nullopt.
TEST(AnnotationReducerUtilTest, AnswerTypeToQueryIntentType_InvalidMapping) {
  EXPECT_EQ(
      AnswerTypeToQueryIntentType(static_cast<ReducedAnswer::AnswerType>(9999)),
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
