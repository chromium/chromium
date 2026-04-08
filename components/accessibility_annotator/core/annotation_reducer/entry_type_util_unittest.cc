// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/entry_type_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/accessibility_annotator/core/data_models/entity.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::UnorderedElementsAreArray;

testing::Matcher<std::vector<EntryMetadata>> MatchesEntryMetadataList(
    const std::vector<EntryMetadata>& expected) {
  return UnorderedElementsAreArray(expected);
}

testing::Matcher<MemorySearchResult> MatchesMemorySearchResult(
    EntryType type,
    const std::u16string& value,
    const std::vector<EntryMetadata>& expected_metadata) {
  return AllOf(
      Field(&MemorySearchResult::type, type),
      Field(&MemorySearchResult::value, value),
      Field(&MemorySearchResult::type_name, std::u16string()),
      Field(&MemorySearchResult::confidence_score, 0.0),
      Field(&MemorySearchResult::sources,
            ElementsAre(MemoryEntrySource(MemoryEntrySourceType::kGmail))),
      Field(&MemorySearchResult::metadata_list,
            MatchesEntryMetadataList(expected_metadata)));
}

TEST(EntryTypeUtilTest, CreateResultFromVehicleEntity) {
  Vehicle vehicle;
  vehicle.make = "Toyota";
  vehicle.model = "Camry";
  vehicle.year = "2020";
  vehicle.owner = "John Doe";
  vehicle.plate_number = "XYZ123";
  vehicle.plate_state = "CA";
  vehicle.vin = "1234567890VIN";

  Entity entity;
  entity.specifics = vehicle;

  EXPECT_THAT(
      CreateResultFromEntity(EntryType::kVehicleMake, entity),
      MatchesMemorySearchResult(
          EntryType::kVehicleMake, u"Toyota",
          {EntryMetadata(EntryType::kNameFull, u"", u"John Doe"),
           EntryMetadata(EntryType::kVehicleModel, u"", u"Camry"),
           EntryMetadata(EntryType::kVehicleOwner, u"", u"John Doe"),
           EntryMetadata(EntryType::kVehiclePlateNumber, u"", u"XYZ123"),
           EntryMetadata(EntryType::kVehiclePlateState, u"", u"CA"),
           EntryMetadata(EntryType::kVehicleVin, u"", u"1234567890VIN"),
           EntryMetadata(EntryType::kVehicleYear, u"", u"2020")}));

  EXPECT_THAT(
      CreateResultFromEntity(EntryType::kVehicle, entity),
      MatchesMemorySearchResult(
          EntryType::kVehicle,
          u"Toyota, Camry, 2020, John Doe, XYZ123, CA, 1234567890VIN",
          {EntryMetadata(EntryType::kNameFull, u"", u"John Doe"),
           EntryMetadata(EntryType::kVehicleMake, u"", u"Toyota"),
           EntryMetadata(EntryType::kVehicleModel, u"", u"Camry"),
           EntryMetadata(EntryType::kVehicleOwner, u"", u"John Doe"),
           EntryMetadata(EntryType::kVehiclePlateNumber, u"", u"XYZ123"),
           EntryMetadata(EntryType::kVehiclePlateState, u"", u"CA"),
           EntryMetadata(EntryType::kVehicleVin, u"", u"1234567890VIN"),
           EntryMetadata(EntryType::kVehicleYear, u"", u"2020")}));
}

TEST(EntryTypeUtilTest, CreateResultFromVehicleEntityWithEmptyAttributes) {
  Vehicle vehicle;
  vehicle.make = "Toyota";
  // model is intentionally empty.
  vehicle.year = "2020";
  // owner is intentionally empty.
  vehicle.plate_number = "XYZ123";
  vehicle.plate_state = "CA";
  vehicle.vin = "1234567890VIN";

  Entity entity;
  entity.specifics = vehicle;

  EXPECT_THAT(
      CreateResultFromEntity(EntryType::kVehicle, entity),
      MatchesMemorySearchResult(
          EntryType::kVehicle, u"Toyota, 2020, XYZ123, CA, 1234567890VIN",
          {EntryMetadata(EntryType::kVehicleMake, u"", u"Toyota"),
           EntryMetadata(EntryType::kVehiclePlateNumber, u"", u"XYZ123"),
           EntryMetadata(EntryType::kVehiclePlateState, u"", u"CA"),
           EntryMetadata(EntryType::kVehicleVin, u"", u"1234567890VIN"),
           EntryMetadata(EntryType::kVehicleYear, u"", u"2020")}));

  EXPECT_THAT(
      CreateResultFromEntity(EntryType::kVehicleMake, entity),
      MatchesMemorySearchResult(
          EntryType::kVehicleMake, u"Toyota",
          {EntryMetadata(EntryType::kVehiclePlateNumber, u"", u"XYZ123"),
           EntryMetadata(EntryType::kVehiclePlateState, u"", u"CA"),
           EntryMetadata(EntryType::kVehicleVin, u"", u"1234567890VIN"),
           EntryMetadata(EntryType::kVehicleYear, u"", u"2020")}));
}

TEST(EntryTypeUtilTest, CreateResultFromPassportEntity) {
  Passport passport;
  passport.name = "John Doe";
  passport.issuing_country = "USA";
  passport.number = "123456789";
  passport.issue_date = Date{15, 2, 2018};
  passport.expiration_date = Date{31, 10, 2028};

  Entity entity;
  entity.specifics = passport;

  EXPECT_THAT(
      CreateResultFromEntity(EntryType::kPassportName, entity),
      MatchesMemorySearchResult(
          EntryType::kPassportName, u"John Doe",
          {EntryMetadata(EntryType::kNameFull, u"", u"John Doe"),
           EntryMetadata(EntryType::kPassportCountry, u"", u"USA"),
           EntryMetadata(EntryType::kPassportExpirationDate, u"",
                         u"2028-10-31"),
           EntryMetadata(EntryType::kPassportIssueDate, u"", u"2018-02-15"),
           EntryMetadata(EntryType::kPassportNumber, u"", u"123456789")}));

  EXPECT_THAT(
      CreateResultFromEntity(EntryType::kPassportFull, entity),
      MatchesMemorySearchResult(
          EntryType::kPassportFull,
          u"John Doe, USA, 123456789, 2018-02-15, 2028-10-31",
          {EntryMetadata(EntryType::kNameFull, u"", u"John Doe"),
           EntryMetadata(EntryType::kPassportCountry, u"", u"USA"),
           EntryMetadata(EntryType::kPassportExpirationDate, u"",
                         u"2028-10-31"),
           EntryMetadata(EntryType::kPassportIssueDate, u"", u"2018-02-15"),
           EntryMetadata(EntryType::kPassportName, u"", u"John Doe"),
           EntryMetadata(EntryType::kPassportNumber, u"", u"123456789")}));
}

TEST(EntryTypeUtilTest, CreateResultFromPassportEntityWithEmptyAttributes) {
  Passport passport;
  passport.name = "John Doe";
  // issuing_country is intentionally empty.
  passport.number = "123456789";
  // issue_date is intentionally empty.
  passport.expiration_date = Date{20, 6, 2025};

  Entity entity;
  entity.specifics = passport;

  EXPECT_THAT(
      CreateResultFromEntity(EntryType::kPassportFull, entity),
      MatchesMemorySearchResult(
          EntryType::kPassportFull, u"John Doe, 123456789, 2025-06-20",
          {EntryMetadata(EntryType::kNameFull, u"", u"John Doe"),
           EntryMetadata(EntryType::kPassportExpirationDate, u"",
                         u"2025-06-20"),
           EntryMetadata(EntryType::kPassportName, u"", u"John Doe"),
           EntryMetadata(EntryType::kPassportNumber, u"", u"123456789")}));

  EXPECT_THAT(
      CreateResultFromEntity(EntryType::kPassportName, entity),
      MatchesMemorySearchResult(
          EntryType::kPassportName, u"John Doe",
          {EntryMetadata(EntryType::kNameFull, u"", u"John Doe"),
           EntryMetadata(EntryType::kPassportExpirationDate, u"",
                         u"2025-06-20"),
           EntryMetadata(EntryType::kPassportNumber, u"", u"123456789")}));
}

TEST(EntryTypeUtilTest, CreateResultFromFlightReservationEntity) {
  FlightReservation flight;
  flight.flight_number = "DL123";
  flight.ticket_number = "0061234567890";
  flight.confirmation_code = "ABCDEF";
  flight.passenger_name = "John Doe";
  flight.departure_airport = "JFK";
  flight.arrival_airport = "LAX";
  base::Time departure_date;
  EXPECT_TRUE(base::Time::FromUTCExploded(
      {.year = 2026, .month = 4, .day_of_week = 1, .day_of_month = 20},
      &departure_date));
  flight.departure_date = departure_date;

  Entity entity;
  entity.specifics = flight;

  EXPECT_THAT(
      CreateResultFromEntity(EntryType::kFlightReservationFlightNumber, entity),
      MatchesMemorySearchResult(
          EntryType::kFlightReservationFlightNumber, u"DL123",
          {EntryMetadata(EntryType::kNameFull, u"", u"John Doe"),
           EntryMetadata(EntryType::kFlightReservationArrivalAirport, u"",
                         u"LAX"),
           EntryMetadata(EntryType::kFlightReservationConfirmationCode, u"",
                         u"ABCDEF"),
           EntryMetadata(EntryType::kFlightReservationDepartureAirport, u"",
                         u"JFK"),
           EntryMetadata(EntryType::kFlightReservationDepartureDate, u"",
                         u"2026-04-20"),
           EntryMetadata(EntryType::kFlightReservationPassengerName, u"",
                         u"John Doe"),
           EntryMetadata(EntryType::kFlightReservationTicketNumber, u"",
                         u"0061234567890")}));

  EXPECT_THAT(
      CreateResultFromEntity(EntryType::kFlightReservationFull, entity),
      MatchesMemorySearchResult(
          EntryType::kFlightReservationFull,
          u"DL123, 0061234567890, ABCDEF, John Doe, JFK, LAX, 2026-04-20",
          {EntryMetadata(EntryType::kNameFull, u"", u"John Doe"),
           EntryMetadata(EntryType::kFlightReservationArrivalAirport, u"",
                         u"LAX"),
           EntryMetadata(EntryType::kFlightReservationConfirmationCode, u"",
                         u"ABCDEF"),
           EntryMetadata(EntryType::kFlightReservationDepartureAirport, u"",
                         u"JFK"),
           EntryMetadata(EntryType::kFlightReservationDepartureDate, u"",
                         u"2026-04-20"),
           EntryMetadata(EntryType::kFlightReservationFlightNumber, u"",
                         u"DL123"),
           EntryMetadata(EntryType::kFlightReservationPassengerName, u"",
                         u"John Doe"),
           EntryMetadata(EntryType::kFlightReservationTicketNumber, u"",
                         u"0061234567890")}));
}

TEST(EntryTypeUtilTest, CreateResultFromNationalIdEntity) {
  NationalId national_id;
  national_id.name = "John Doe";
  national_id.number = "123456789";
  national_id.issuing_country = "USA";
  national_id.issue_date = Date{4, 3, 2015};
  national_id.expiration_date = Date{3, 3, 2025};

  Entity entity;
  entity.specifics = national_id;

  EXPECT_THAT(
      CreateResultFromEntity(EntryType::kNationalIdCardNumber, entity),
      MatchesMemorySearchResult(
          EntryType::kNationalIdCardNumber, u"123456789",
          {EntryMetadata(EntryType::kNameFull, u"", u"John Doe"),
           EntryMetadata(EntryType::kNationalIdCardCountry, u"", u"USA"),
           EntryMetadata(EntryType::kNationalIdCardExpirationDate, u"",
                         u"2025-03-03"),
           EntryMetadata(EntryType::kNationalIdCardIssueDate, u"",
                         u"2015-03-04"),
           EntryMetadata(EntryType::kNationalIdCardName, u"", u"John Doe")}));

  EXPECT_THAT(
      CreateResultFromEntity(EntryType::kNationalIdCardFull, entity),
      MatchesMemorySearchResult(
          EntryType::kNationalIdCardFull,
          u"John Doe, USA, 123456789, 2015-03-04, 2025-03-03",
          {EntryMetadata(EntryType::kNameFull, u"", u"John Doe"),
           EntryMetadata(EntryType::kNationalIdCardCountry, u"", u"USA"),
           EntryMetadata(EntryType::kNationalIdCardExpirationDate, u"",
                         u"2025-03-03"),
           EntryMetadata(EntryType::kNationalIdCardIssueDate, u"",
                         u"2015-03-04"),
           EntryMetadata(EntryType::kNationalIdCardName, u"", u"John Doe"),
           EntryMetadata(EntryType::kNationalIdCardNumber, u"",
                         u"123456789")}));
}

TEST(EntryTypeUtilTest, CreateResultFromDriversLicenseEntity) {
  DriversLicense dl;
  dl.name = "John Doe";
  dl.number = "D1234567";
  dl.state = "CA";
  dl.issue_date = Date{12, 8, 2019};
  dl.expiration_date = Date{12, 8, 2024};

  Entity entity;
  entity.specifics = dl;

  EXPECT_THAT(
      CreateResultFromEntity(EntryType::kDriversLicenseNumber, entity),
      MatchesMemorySearchResult(
          EntryType::kDriversLicenseNumber, u"D1234567",
          {EntryMetadata(EntryType::kNameFull, u"", u"John Doe"),
           EntryMetadata(EntryType::kDriversLicenseExpirationDate, u"",
                         u"2024-08-12"),
           EntryMetadata(EntryType::kDriversLicenseIssueDate, u"",
                         u"2019-08-12"),
           EntryMetadata(EntryType::kDriversLicenseName, u"", u"John Doe"),
           EntryMetadata(EntryType::kDriversLicenseState, u"", u"CA")}));

  EXPECT_THAT(
      CreateResultFromEntity(EntryType::kDriversLicenseFull, entity),
      MatchesMemorySearchResult(
          EntryType::kDriversLicenseFull,
          u"John Doe, CA, D1234567, 2019-08-12, 2024-08-12",
          {EntryMetadata(EntryType::kNameFull, u"", u"John Doe"),
           EntryMetadata(EntryType::kDriversLicenseExpirationDate, u"",
                         u"2024-08-12"),
           EntryMetadata(EntryType::kDriversLicenseIssueDate, u"",
                         u"2019-08-12"),
           EntryMetadata(EntryType::kDriversLicenseName, u"", u"John Doe"),
           EntryMetadata(EntryType::kDriversLicenseNumber, u"", u"D1234567"),
           EntryMetadata(EntryType::kDriversLicenseState, u"", u"CA")}));
}

TEST(EntryTypeUtilTest, CreateResultFromOrderEntity) {
  Order order;
  order.id = "ORD12345";
  order.account = "john.doe@example.com";
  order.merchant_name = "Amazon";
  order.grand_total = "$100.00";
  order.order_date = Date{23, 11, 2023};
  Order::ItemDescription item1;
  item1.name = "Laptop";
  Order::ItemDescription item2;
  item2.name = "Mouse";
  order.products = {item1, item2};

  Entity entity;
  entity.specifics = order;

  EXPECT_THAT(CreateResultFromEntity(EntryType::kOrderId, entity),
              MatchesMemorySearchResult(
                  EntryType::kOrderId, u"ORD12345",
                  {EntryMetadata(EntryType::kOrderAccount, u"",
                                 u"john.doe@example.com"),
                   EntryMetadata(EntryType::kOrderDate, u"", u"2023-11-23"),
                   EntryMetadata(EntryType::kOrderGrandTotal, u"", u"$100.00"),
                   EntryMetadata(EntryType::kOrderMerchantName, u"", u"Amazon"),
                   EntryMetadata(EntryType::kOrderProductNames, u"",
                                 u"Laptop, Mouse")}));

  EXPECT_THAT(
      CreateResultFromEntity(EntryType::kOrderFull, entity),
      MatchesMemorySearchResult(
          EntryType::kOrderFull,
          u"ORD12345, john.doe@example.com, Amazon, Laptop, Mouse, $100.00, "
          u"2023-11-23",
          {EntryMetadata(EntryType::kOrderAccount, u"",
                         u"john.doe@example.com"),
           EntryMetadata(EntryType::kOrderDate, u"", u"2023-11-23"),
           EntryMetadata(EntryType::kOrderGrandTotal, u"", u"$100.00"),
           EntryMetadata(EntryType::kOrderId, u"", u"ORD12345"),
           EntryMetadata(EntryType::kOrderMerchantName, u"", u"Amazon"),
           EntryMetadata(EntryType::kOrderProductNames, u"",
                         u"Laptop, Mouse")}));
}

TEST(EntryTypeUtilTest, CreateResultFromOrderEntityWithEmptyAttributes) {
  Order order;
  order.id = "ORD12345";
  order.account = "john.doe@example.com";
  // merchant_name is intentionally empty.
  order.grand_total = "$100.00";

  Entity entity;
  entity.specifics = order;

  EXPECT_THAT(
      CreateResultFromEntity(EntryType::kOrderFull, entity),
      MatchesMemorySearchResult(
          EntryType::kOrderFull, u"ORD12345, john.doe@example.com, $100.00",
          {EntryMetadata(EntryType::kOrderAccount, u"",
                         u"john.doe@example.com"),
           EntryMetadata(EntryType::kOrderGrandTotal, u"", u"$100.00"),
           EntryMetadata(EntryType::kOrderId, u"", u"ORD12345")}));

  EXPECT_THAT(
      CreateResultFromEntity(EntryType::kOrderId, entity),
      MatchesMemorySearchResult(
          EntryType::kOrderId, u"ORD12345",
          {EntryMetadata(EntryType::kOrderAccount, u"",
                         u"john.doe@example.com"),
           EntryMetadata(EntryType::kOrderGrandTotal, u"", u"$100.00")}));
}

TEST(EntryTypeUtilTest, CreateResultFromShipmentEntity) {
  Shipment shipment;
  shipment.tracking_number = "1Z9999999999999999";
  shipment.associated_order_id = "ORD123";
  shipment.delivery_address = "123 Main St";
  shipment.carrier_name = "UPS";
  shipment.carrier_domain = GURL("https://www.example.com");
  shipment.estimated_delivery_date = Date{15, 6, 2026};

  Entity entity;
  entity.specifics = shipment;

  EXPECT_THAT(
      CreateResultFromEntity(EntryType::kShipmentTrackingNumber, entity),
      MatchesMemorySearchResult(
          EntryType::kShipmentTrackingNumber, u"1Z9999999999999999",
          {EntryMetadata(EntryType::kShipmentAssociatedOrderId, u"", u"ORD123"),
           EntryMetadata(EntryType::kShipmentCarrierDomain, u"",
                         u"https://www.example.com/"),
           EntryMetadata(EntryType::kShipmentCarrierName, u"", u"UPS"),
           EntryMetadata(EntryType::kShipmentDeliveryAddress, u"",
                         u"123 Main St"),
           EntryMetadata(EntryType::kShipmentEstimatedDeliveryDate, u"",
                         u"2026-06-15")}));

  EXPECT_THAT(
      CreateResultFromEntity(EntryType::kShipmentFull, entity),
      MatchesMemorySearchResult(
          EntryType::kShipmentFull,
          u"1Z9999999999999999, ORD123, 123 Main St, UPS, "
          u"https://www.example.com/, 2026-06-15",
          {EntryMetadata(EntryType::kShipmentAssociatedOrderId, u"", u"ORD123"),
           EntryMetadata(EntryType::kShipmentCarrierDomain, u"",
                         u"https://www.example.com/"),
           EntryMetadata(EntryType::kShipmentCarrierName, u"", u"UPS"),
           EntryMetadata(EntryType::kShipmentDeliveryAddress, u"",
                         u"123 Main St"),
           EntryMetadata(EntryType::kShipmentEstimatedDeliveryDate, u"",
                         u"2026-06-15"),
           EntryMetadata(EntryType::kShipmentTrackingNumber, u"",
                         u"1Z9999999999999999")}));
}

TEST(EntryTypeUtilTest, GetEntityTypesForEntryType) {
  EXPECT_EQ(GetEntityTypesForEntryType(EntryType::kVehicleMake),
            EntityTypeEnumSet({EntityType::kVehicle}));

  EXPECT_EQ(GetEntityTypesForEntryType(EntryType::kPassportName),
            EntityTypeEnumSet({EntityType::kPassport}));

  EXPECT_EQ(
      GetEntityTypesForEntryType(EntryType::kFlightReservationFlightNumber),
      EntityTypeEnumSet({EntityType::kFlightReservation}));

  EXPECT_EQ(GetEntityTypesForEntryType(EntryType::kNationalIdCardNumber),
            EntityTypeEnumSet({EntityType::kNationalId}));

  EXPECT_EQ(GetEntityTypesForEntryType(EntryType::kDriversLicenseNumber),
            EntityTypeEnumSet({EntityType::kDriversLicense}));

  EXPECT_EQ(GetEntityTypesForEntryType(EntryType::kOrderId),
            EntityTypeEnumSet({EntityType::kOrder}));

  EXPECT_EQ(GetEntityTypesForEntryType(EntryType::kShipmentTrackingNumber),
            EntityTypeEnumSet({EntityType::kShipment}));

  EXPECT_EQ(GetEntityTypesForEntryType(EntryType::kNameFull),
            EntityTypeEnumSet(
                {EntityType::kVehicle, EntityType::kPassport,
                 EntityType::kFlightReservation, EntityType::kNationalId,
                 EntityType::kDriversLicense, EntityType::kOrder}));
  EXPECT_EQ(GetEntityTypesForEntryType(EntryType::kUnknown),
            EntityTypeEnumSet({EntityType::kUnknown}));
}

}  // namespace

}  // namespace accessibility_annotator
