// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/util.h"

#include <optional>

#include "components/accessibility_annotator/core/annotation_reducer/memory_data_type.h"
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
// corresponding MemoryDataType enums.
TEST(AnnotationReducerUtilTest, StringToMemoryDataType_ValidMappings) {
  EXPECT_EQ(StringToMemoryDataType("kAddressCity"),
            MemoryDataType::kAddressCity);
  EXPECT_EQ(StringToMemoryDataType("kAddressCountry"),
            MemoryDataType::kAddressCountry);
  EXPECT_EQ(StringToMemoryDataType("kAddressFull"),
            MemoryDataType::kAddressFull);
  EXPECT_EQ(StringToMemoryDataType("kAddressState"),
            MemoryDataType::kAddressState);
  EXPECT_EQ(StringToMemoryDataType("kAddressStreetAddress"),
            MemoryDataType::kAddressStreetAddress);
  EXPECT_EQ(StringToMemoryDataType("kAddressZip"), MemoryDataType::kAddressZip);
  EXPECT_EQ(StringToMemoryDataType("kCompanyName"),
            MemoryDataType::kCompanyName);
  EXPECT_EQ(StringToMemoryDataType("kCreditCardExpirationDate"),
            MemoryDataType::kCreditCardExpirationDate);
  EXPECT_EQ(StringToMemoryDataType("kCreditCardNameOnCard"),
            MemoryDataType::kCreditCardNameOnCard);
  EXPECT_EQ(StringToMemoryDataType("kCreditCardNickname"),
            MemoryDataType::kCreditCardNickname);
  EXPECT_EQ(StringToMemoryDataType("kCreditCardNumber"),
            MemoryDataType::kCreditCardNumber);
  EXPECT_EQ(StringToMemoryDataType("kCreditCardSecurityCode"),
            MemoryDataType::kCreditCardSecurityCode);
  EXPECT_EQ(StringToMemoryDataType("kDriversLicenseExpirationDate"),
            MemoryDataType::kDriversLicenseExpirationDate);
  EXPECT_EQ(StringToMemoryDataType("kDriversLicenseFull"),
            MemoryDataType::kDriversLicenseFull);
  EXPECT_EQ(StringToMemoryDataType("kDriversLicenseIssueDate"),
            MemoryDataType::kDriversLicenseIssueDate);
  EXPECT_EQ(StringToMemoryDataType("kDriversLicenseName"),
            MemoryDataType::kDriversLicenseName);
  EXPECT_EQ(StringToMemoryDataType("kDriversLicenseNumber"),
            MemoryDataType::kDriversLicenseNumber);
  EXPECT_EQ(StringToMemoryDataType("kDriversLicenseState"),
            MemoryDataType::kDriversLicenseState);
  EXPECT_EQ(StringToMemoryDataType("kEmail"), MemoryDataType::kEmail);
  EXPECT_EQ(StringToMemoryDataType("kFlightReservationArrivalAirport"),
            MemoryDataType::kFlightReservationArrivalAirport);
  EXPECT_EQ(StringToMemoryDataType("kFlightReservationConfirmationCode"),
            MemoryDataType::kFlightReservationConfirmationCode);
  EXPECT_EQ(StringToMemoryDataType("kFlightReservationDepartureAirport"),
            MemoryDataType::kFlightReservationDepartureAirport);
  EXPECT_EQ(StringToMemoryDataType("kFlightReservationDepartureDate"),
            MemoryDataType::kFlightReservationDepartureDate);
  EXPECT_EQ(StringToMemoryDataType("kFlightReservationFlightNumber"),
            MemoryDataType::kFlightReservationFlightNumber);
  EXPECT_EQ(StringToMemoryDataType("kFlightReservationFull"),
            MemoryDataType::kFlightReservationFull);
  EXPECT_EQ(StringToMemoryDataType("kFlightReservationPassengerName"),
            MemoryDataType::kFlightReservationPassengerName);
  EXPECT_EQ(StringToMemoryDataType("kFlightReservationTicketNumber"),
            MemoryDataType::kFlightReservationTicketNumber);
  EXPECT_EQ(StringToMemoryDataType("kIban"), MemoryDataType::kIban);
  EXPECT_EQ(StringToMemoryDataType("kIbanNickname"),
            MemoryDataType::kIbanNickname);
  EXPECT_EQ(StringToMemoryDataType("kKnownTravelerNumberExpirationDate"),
            MemoryDataType::kKnownTravelerNumberExpirationDate);
  EXPECT_EQ(StringToMemoryDataType("kKnownTravelerNumberFull"),
            MemoryDataType::kKnownTravelerNumberFull);
  EXPECT_EQ(StringToMemoryDataType("kKnownTravelerNumberName"),
            MemoryDataType::kKnownTravelerNumberName);
  EXPECT_EQ(StringToMemoryDataType("kKnownTravelerNumberNumber"),
            MemoryDataType::kKnownTravelerNumberNumber);
  EXPECT_EQ(StringToMemoryDataType("kNameFull"), MemoryDataType::kNameFull);
  EXPECT_EQ(StringToMemoryDataType("kNationalIdCardCountry"),
            MemoryDataType::kNationalIdCardCountry);
  EXPECT_EQ(StringToMemoryDataType("kNationalIdCardExpirationDate"),
            MemoryDataType::kNationalIdCardExpirationDate);
  EXPECT_EQ(StringToMemoryDataType("kNationalIdCardFull"),
            MemoryDataType::kNationalIdCardFull);
  EXPECT_EQ(StringToMemoryDataType("kNationalIdCardIssueDate"),
            MemoryDataType::kNationalIdCardIssueDate);
  EXPECT_EQ(StringToMemoryDataType("kNationalIdCardName"),
            MemoryDataType::kNationalIdCardName);
  EXPECT_EQ(StringToMemoryDataType("kNationalIdCardNumber"),
            MemoryDataType::kNationalIdCardNumber);
  EXPECT_EQ(StringToMemoryDataType("kOrderAccount"),
            MemoryDataType::kOrderAccount);
  EXPECT_EQ(StringToMemoryDataType("kOrderDate"), MemoryDataType::kOrderDate);
  EXPECT_EQ(StringToMemoryDataType("kOrderFull"), MemoryDataType::kOrderFull);
  EXPECT_EQ(StringToMemoryDataType("kOrderGrandTotal"),
            MemoryDataType::kOrderGrandTotal);
  EXPECT_EQ(StringToMemoryDataType("kOrderId"), MemoryDataType::kOrderId);
  EXPECT_EQ(StringToMemoryDataType("kOrderMerchantDomain"),
            MemoryDataType::kOrderMerchantDomain);
  EXPECT_EQ(StringToMemoryDataType("kOrderMerchantName"),
            MemoryDataType::kOrderMerchantName);
  EXPECT_EQ(StringToMemoryDataType("kOrderProductNames"),
            MemoryDataType::kOrderProductNames);
  EXPECT_EQ(StringToMemoryDataType("kPassportCountry"),
            MemoryDataType::kPassportCountry);
  EXPECT_EQ(StringToMemoryDataType("kPassportExpirationDate"),
            MemoryDataType::kPassportExpirationDate);
  EXPECT_EQ(StringToMemoryDataType("kPassportFull"),
            MemoryDataType::kPassportFull);
  EXPECT_EQ(StringToMemoryDataType("kPassportIssueDate"),
            MemoryDataType::kPassportIssueDate);
  EXPECT_EQ(StringToMemoryDataType("kPassportName"),
            MemoryDataType::kPassportName);
  EXPECT_EQ(StringToMemoryDataType("kPassportNumber"),
            MemoryDataType::kPassportNumber);
  EXPECT_EQ(StringToMemoryDataType("kPhone"), MemoryDataType::kPhone);
  EXPECT_EQ(StringToMemoryDataType("kRedressNumberFull"),
            MemoryDataType::kRedressNumberFull);
  EXPECT_EQ(StringToMemoryDataType("kRedressNumberName"),
            MemoryDataType::kRedressNumberName);
  EXPECT_EQ(StringToMemoryDataType("kRedressNumberNumber"),
            MemoryDataType::kRedressNumberNumber);
  EXPECT_EQ(StringToMemoryDataType("kVehicle"), MemoryDataType::kVehicle);
  EXPECT_EQ(StringToMemoryDataType("kVehicleMake"),
            MemoryDataType::kVehicleMake);
  EXPECT_EQ(StringToMemoryDataType("kVehicleModel"),
            MemoryDataType::kVehicleModel);
  EXPECT_EQ(StringToMemoryDataType("kVehicleOwner"),
            MemoryDataType::kVehicleOwner);
  EXPECT_EQ(StringToMemoryDataType("kVehiclePlateNumber"),
            MemoryDataType::kVehiclePlateNumber);
  EXPECT_EQ(StringToMemoryDataType("kVehiclePlateState"),
            MemoryDataType::kVehiclePlateState);
  EXPECT_EQ(StringToMemoryDataType("kVehicleVin"), MemoryDataType::kVehicleVin);
  EXPECT_EQ(StringToMemoryDataType("kVehicleYear"),
            MemoryDataType::kVehicleYear);
}

// Verifies that an unrecognized or empty string intent is safely mapped to
// kUnknown.
TEST(AnnotationReducerUtilTest, StringToMemoryDataType_InvalidMapping) {
  EXPECT_EQ(StringToMemoryDataType("kNonExistentIntent"),
            MemoryDataType::kUnknown);
  EXPECT_EQ(StringToMemoryDataType(""), MemoryDataType::kUnknown);
}

// Verifies that all valid AnswerType proto enums correctly map to
// MemoryDataType enums.
TEST(AnnotationReducerUtilTest, AnswerTypeToMemoryDataType_ValidMappings) {
  EXPECT_EQ(AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_UNKNOWN),
            MemoryDataType::kUnknown);
  EXPECT_EQ(AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_NAME_FULL),
            MemoryDataType::kNameFull);
  EXPECT_EQ(AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_ADDRESS_FULL),
            MemoryDataType::kAddressFull);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_ADDRESS_STREET_ADDRESS),
            MemoryDataType::kAddressStreetAddress);
  EXPECT_EQ(AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_ADDRESS_CITY),
            MemoryDataType::kAddressCity);
  EXPECT_EQ(
      AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_ADDRESS_STATE),
      MemoryDataType::kAddressState);
  EXPECT_EQ(AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_ADDRESS_ZIP),
            MemoryDataType::kAddressZip);
  EXPECT_EQ(
      AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_ADDRESS_COUNTRY),
      MemoryDataType::kAddressCountry);
  EXPECT_EQ(AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_PHONE),
            MemoryDataType::kPhone);
  EXPECT_EQ(AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_EMAIL),
            MemoryDataType::kEmail);
  EXPECT_EQ(AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_COMPANY_NAME),
            MemoryDataType::kCompanyName);
  EXPECT_EQ(AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_IBAN),
            MemoryDataType::kIban);
  EXPECT_EQ(
      AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_IBAN_NICKNAME),
      MemoryDataType::kIbanNickname);
  EXPECT_EQ(AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_VEHICLE),
            MemoryDataType::kVehicle);
  EXPECT_EQ(AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_VEHICLE_MAKE),
            MemoryDataType::kVehicleMake);
  EXPECT_EQ(
      AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_VEHICLE_MODEL),
      MemoryDataType::kVehicleModel);
  EXPECT_EQ(AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_VEHICLE_YEAR),
            MemoryDataType::kVehicleYear);
  EXPECT_EQ(
      AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_VEHICLE_OWNER),
      MemoryDataType::kVehicleOwner);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_VEHICLE_PLATE_NUMBER),
            MemoryDataType::kVehiclePlateNumber);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_VEHICLE_PLATE_STATE),
            MemoryDataType::kVehiclePlateState);
  EXPECT_EQ(AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_VEHICLE_VIN),
            MemoryDataType::kVehicleVin);
  EXPECT_EQ(
      AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_PASSPORT_FULL),
      MemoryDataType::kPassportFull);
  EXPECT_EQ(
      AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_PASSPORT_NAME),
      MemoryDataType::kPassportName);
  EXPECT_EQ(
      AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_PASSPORT_COUNTRY),
      MemoryDataType::kPassportCountry);
  EXPECT_EQ(
      AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_PASSPORT_NUMBER),
      MemoryDataType::kPassportNumber);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_PASSPORT_ISSUE_DATE),
            MemoryDataType::kPassportIssueDate);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_PASSPORT_EXPIRATION_DATE),
            MemoryDataType::kPassportExpirationDate);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_FULL),
            MemoryDataType::kFlightReservationFull);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_FLIGHT_NUMBER),
            MemoryDataType::kFlightReservationFlightNumber);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_TICKET_NUMBER),
            MemoryDataType::kFlightReservationTicketNumber);
  EXPECT_EQ(
      AnswerTypeToMemoryDataType(
          ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_CONFIRMATION_CODE),
      MemoryDataType::kFlightReservationConfirmationCode);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_PASSENGER_NAME),
            MemoryDataType::kFlightReservationPassengerName);
  EXPECT_EQ(
      AnswerTypeToMemoryDataType(
          ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_DEPARTURE_AIRPORT),
      MemoryDataType::kFlightReservationDepartureAirport);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_ARRIVAL_AIRPORT),
            MemoryDataType::kFlightReservationArrivalAirport);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_DEPARTURE_DATE),
            MemoryDataType::kFlightReservationDepartureDate);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_FLIGHT_RESERVATION_ARRIVAL_DATE),
            MemoryDataType::kFlightReservationArrivalDate);
  EXPECT_EQ(
      AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_SHIPMENT_FULL),
      MemoryDataType::kShipmentFull);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_SHIPMENT_TRACKING_NUMBER),
            MemoryDataType::kShipmentTrackingNumber);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_SHIPMENT_ASSOCIATED_ORDER_ID),
            MemoryDataType::kShipmentAssociatedOrderId);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_SHIPMENT_DELIVERY_ADDRESS),
            MemoryDataType::kShipmentDeliveryAddress);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_SHIPMENT_CARRIER_NAME),
            MemoryDataType::kShipmentCarrierName);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_SHIPMENT_CARRIER_DOMAIN),
            MemoryDataType::kShipmentCarrierDomain);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_SHIPMENT_ESTIMATED_DELIVERY_DATE),
            MemoryDataType::kShipmentEstimatedDeliveryDate);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_NATIONAL_ID_CARD_FULL),
            MemoryDataType::kNationalIdCardFull);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_NATIONAL_ID_CARD_NAME),
            MemoryDataType::kNationalIdCardName);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_NATIONAL_ID_CARD_COUNTRY),
            MemoryDataType::kNationalIdCardCountry);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_NATIONAL_ID_CARD_NUMBER),
            MemoryDataType::kNationalIdCardNumber);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_NATIONAL_ID_CARD_ISSUE_DATE),
            MemoryDataType::kNationalIdCardIssueDate);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_NATIONAL_ID_CARD_EXPIRATION_DATE),
            MemoryDataType::kNationalIdCardExpirationDate);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_REDRESS_NUMBER_FULL),
            MemoryDataType::kRedressNumberFull);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_REDRESS_NUMBER_NAME),
            MemoryDataType::kRedressNumberName);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_REDRESS_NUMBER_NUMBER),
            MemoryDataType::kRedressNumberNumber);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_FULL),
            MemoryDataType::kKnownTravelerNumberFull);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_NAME),
            MemoryDataType::kKnownTravelerNumberName);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_NUMBER),
            MemoryDataType::kKnownTravelerNumberNumber);
  EXPECT_EQ(
      AnswerTypeToMemoryDataType(
          ReducedAnswer::ANSWER_TYPE_KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE),
      MemoryDataType::kKnownTravelerNumberExpirationDate);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_DRIVERS_LICENSE_FULL),
            MemoryDataType::kDriversLicenseFull);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_DRIVERS_LICENSE_NAME),
            MemoryDataType::kDriversLicenseName);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_DRIVERS_LICENSE_STATE),
            MemoryDataType::kDriversLicenseState);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_DRIVERS_LICENSE_NUMBER),
            MemoryDataType::kDriversLicenseNumber);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_DRIVERS_LICENSE_ISSUE_DATE),
            MemoryDataType::kDriversLicenseIssueDate);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_DRIVERS_LICENSE_EXPIRATION_DATE),
            MemoryDataType::kDriversLicenseExpirationDate);
  EXPECT_EQ(AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_ORDER_FULL),
            MemoryDataType::kOrderFull);
  EXPECT_EQ(AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_ORDER_ID),
            MemoryDataType::kOrderId);
  EXPECT_EQ(
      AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_ORDER_ACCOUNT),
      MemoryDataType::kOrderAccount);
  EXPECT_EQ(AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_ORDER_DATE),
            MemoryDataType::kOrderDate);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_ORDER_MERCHANT_NAME),
            MemoryDataType::kOrderMerchantName);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_ORDER_MERCHANT_DOMAIN),
            MemoryDataType::kOrderMerchantDomain);
  EXPECT_EQ(AnswerTypeToMemoryDataType(
                ReducedAnswer::ANSWER_TYPE_ORDER_PRODUCT_NAMES),
            MemoryDataType::kOrderProductNames);
  EXPECT_EQ(
      AnswerTypeToMemoryDataType(ReducedAnswer::ANSWER_TYPE_ORDER_GRAND_TOTAL),
      MemoryDataType::kOrderGrandTotal);
}

// Verifies that an out-of-bounds AnswerType proto enum maps to std::nullopt.
TEST(AnnotationReducerUtilTest, AnswerTypeToMemoryDataType_InvalidMapping) {
  EXPECT_EQ(
      AnswerTypeToMemoryDataType(static_cast<ReducedAnswer::AnswerType>(9999)),
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
