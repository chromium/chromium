// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/memory_data_type_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/accessibility_annotator/core/data_models/entity.h"
#include "components/personal_context/proto/features/common_data.pb.h"
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
    MemoryDataType type,
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

TEST(MemoryDataTypeUtilTest, CreateResultFromVehicleEntity) {
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
      CreateResultFromEntity(MemoryDataType::kVehicleMake, entity),
      MatchesMemorySearchResult(
          MemoryDataType::kVehicleMake, u"Toyota",
          {EntryMetadata(MemoryDataType::kNameFull, u"", u"John Doe"),
           EntryMetadata(MemoryDataType::kVehicleModel, u"", u"Camry"),
           EntryMetadata(MemoryDataType::kVehicleOwner, u"", u"John Doe"),
           EntryMetadata(MemoryDataType::kVehiclePlateNumber, u"", u"XYZ123"),
           EntryMetadata(MemoryDataType::kVehiclePlateState, u"", u"CA"),
           EntryMetadata(MemoryDataType::kVehicleVin, u"", u"1234567890VIN"),
           EntryMetadata(MemoryDataType::kVehicleYear, u"", u"2020")}));

  EXPECT_THAT(
      CreateResultFromEntity(MemoryDataType::kVehicle, entity),
      MatchesMemorySearchResult(
          MemoryDataType::kVehicle,
          u"Toyota, Camry, 2020, John Doe, XYZ123, CA, 1234567890VIN",
          {EntryMetadata(MemoryDataType::kNameFull, u"", u"John Doe"),
           EntryMetadata(MemoryDataType::kVehicleMake, u"", u"Toyota"),
           EntryMetadata(MemoryDataType::kVehicleModel, u"", u"Camry"),
           EntryMetadata(MemoryDataType::kVehicleOwner, u"", u"John Doe"),
           EntryMetadata(MemoryDataType::kVehiclePlateNumber, u"", u"XYZ123"),
           EntryMetadata(MemoryDataType::kVehiclePlateState, u"", u"CA"),
           EntryMetadata(MemoryDataType::kVehicleVin, u"", u"1234567890VIN"),
           EntryMetadata(MemoryDataType::kVehicleYear, u"", u"2020")}));
}

TEST(MemoryDataTypeUtilTest, CreateResultFromVehicleEntityWithEmptyAttributes) {
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
      CreateResultFromEntity(MemoryDataType::kVehicle, entity),
      MatchesMemorySearchResult(
          MemoryDataType::kVehicle, u"Toyota, 2020, XYZ123, CA, 1234567890VIN",
          {EntryMetadata(MemoryDataType::kVehicleMake, u"", u"Toyota"),
           EntryMetadata(MemoryDataType::kVehiclePlateNumber, u"", u"XYZ123"),
           EntryMetadata(MemoryDataType::kVehiclePlateState, u"", u"CA"),
           EntryMetadata(MemoryDataType::kVehicleVin, u"", u"1234567890VIN"),
           EntryMetadata(MemoryDataType::kVehicleYear, u"", u"2020")}));

  EXPECT_THAT(
      CreateResultFromEntity(MemoryDataType::kVehicleMake, entity),
      MatchesMemorySearchResult(
          MemoryDataType::kVehicleMake, u"Toyota",
          {EntryMetadata(MemoryDataType::kVehiclePlateNumber, u"", u"XYZ123"),
           EntryMetadata(MemoryDataType::kVehiclePlateState, u"", u"CA"),
           EntryMetadata(MemoryDataType::kVehicleVin, u"", u"1234567890VIN"),
           EntryMetadata(MemoryDataType::kVehicleYear, u"", u"2020")}));
}

TEST(MemoryDataTypeUtilTest, CreateResultFromPassportEntity) {
  Passport passport;
  passport.name = "John Doe";
  passport.issuing_country = "USA";
  passport.number = "123456789";
  passport.issue_date = Date{15, 2, 2018};
  passport.expiration_date = Date{31, 10, 2028};

  Entity entity;
  entity.specifics = passport;

  EXPECT_THAT(
      CreateResultFromEntity(MemoryDataType::kPassportName, entity),
      MatchesMemorySearchResult(
          MemoryDataType::kPassportName, u"John Doe",
          {EntryMetadata(MemoryDataType::kNameFull, u"", u"John Doe"),
           EntryMetadata(MemoryDataType::kPassportCountry, u"", u"USA"),
           EntryMetadata(MemoryDataType::kPassportExpirationDate, u"",
                         u"2028-10-31"),
           EntryMetadata(MemoryDataType::kPassportIssueDate, u"",
                         u"2018-02-15"),
           EntryMetadata(MemoryDataType::kPassportNumber, u"", u"123456789")}));

  EXPECT_THAT(
      CreateResultFromEntity(MemoryDataType::kPassportFull, entity),
      MatchesMemorySearchResult(
          MemoryDataType::kPassportFull,
          u"John Doe, USA, 123456789, 2018-02-15, 2028-10-31",
          {EntryMetadata(MemoryDataType::kNameFull, u"", u"John Doe"),
           EntryMetadata(MemoryDataType::kPassportCountry, u"", u"USA"),
           EntryMetadata(MemoryDataType::kPassportExpirationDate, u"",
                         u"2028-10-31"),
           EntryMetadata(MemoryDataType::kPassportIssueDate, u"",
                         u"2018-02-15"),
           EntryMetadata(MemoryDataType::kPassportName, u"", u"John Doe"),
           EntryMetadata(MemoryDataType::kPassportNumber, u"", u"123456789")}));
}

TEST(MemoryDataTypeUtilTest,
     CreateResultFromPassportEntityWithEmptyAttributes) {
  Passport passport;
  passport.name = "John Doe";
  // issuing_country is intentionally empty.
  passport.number = "123456789";
  // issue_date is intentionally empty.
  passport.expiration_date = Date{20, 6, 2025};

  Entity entity;
  entity.specifics = passport;

  EXPECT_THAT(
      CreateResultFromEntity(MemoryDataType::kPassportFull, entity),
      MatchesMemorySearchResult(
          MemoryDataType::kPassportFull, u"John Doe, 123456789, 2025-06-20",
          {EntryMetadata(MemoryDataType::kNameFull, u"", u"John Doe"),
           EntryMetadata(MemoryDataType::kPassportExpirationDate, u"",
                         u"2025-06-20"),
           EntryMetadata(MemoryDataType::kPassportName, u"", u"John Doe"),
           EntryMetadata(MemoryDataType::kPassportNumber, u"", u"123456789")}));

  EXPECT_THAT(
      CreateResultFromEntity(MemoryDataType::kPassportName, entity),
      MatchesMemorySearchResult(
          MemoryDataType::kPassportName, u"John Doe",
          {EntryMetadata(MemoryDataType::kNameFull, u"", u"John Doe"),
           EntryMetadata(MemoryDataType::kPassportExpirationDate, u"",
                         u"2025-06-20"),
           EntryMetadata(MemoryDataType::kPassportNumber, u"", u"123456789")}));
}

TEST(MemoryDataTypeUtilTest, CreateResultFromFlightReservationEntity) {
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
      CreateResultFromEntity(MemoryDataType::kFlightReservationFlightNumber,
                             entity),
      MatchesMemorySearchResult(
          MemoryDataType::kFlightReservationFlightNumber, u"DL123",
          {EntryMetadata(MemoryDataType::kNameFull, u"", u"John Doe"),
           EntryMetadata(MemoryDataType::kFlightReservationArrivalAirport, u"",
                         u"LAX"),
           EntryMetadata(MemoryDataType::kFlightReservationConfirmationCode,
                         u"", u"ABCDEF"),
           EntryMetadata(MemoryDataType::kFlightReservationDepartureAirport,
                         u"", u"JFK"),
           EntryMetadata(MemoryDataType::kFlightReservationDepartureDate, u"",
                         u"2026-04-20"),
           EntryMetadata(MemoryDataType::kFlightReservationPassengerName, u"",
                         u"John Doe"),
           EntryMetadata(MemoryDataType::kFlightReservationTicketNumber, u"",
                         u"0061234567890")}));

  EXPECT_THAT(
      CreateResultFromEntity(MemoryDataType::kFlightReservationFull, entity),
      MatchesMemorySearchResult(
          MemoryDataType::kFlightReservationFull,
          u"DL123, 0061234567890, ABCDEF, John Doe, JFK, LAX, 2026-04-20",
          {EntryMetadata(MemoryDataType::kNameFull, u"", u"John Doe"),
           EntryMetadata(MemoryDataType::kFlightReservationArrivalAirport, u"",
                         u"LAX"),
           EntryMetadata(MemoryDataType::kFlightReservationConfirmationCode,
                         u"", u"ABCDEF"),
           EntryMetadata(MemoryDataType::kFlightReservationDepartureAirport,
                         u"", u"JFK"),
           EntryMetadata(MemoryDataType::kFlightReservationDepartureDate, u"",
                         u"2026-04-20"),
           EntryMetadata(MemoryDataType::kFlightReservationFlightNumber, u"",
                         u"DL123"),
           EntryMetadata(MemoryDataType::kFlightReservationPassengerName, u"",
                         u"John Doe"),
           EntryMetadata(MemoryDataType::kFlightReservationTicketNumber, u"",
                         u"0061234567890")}));
}

TEST(MemoryDataTypeUtilTest, CreateResultFromNationalIdEntity) {
  NationalId national_id;
  national_id.name = "John Doe";
  national_id.number = "123456789";
  national_id.issuing_country = "USA";
  national_id.issue_date = Date{4, 3, 2015};
  national_id.expiration_date = Date{3, 3, 2025};

  Entity entity;
  entity.specifics = national_id;

  EXPECT_THAT(
      CreateResultFromEntity(MemoryDataType::kNationalIdCardNumber, entity),
      MatchesMemorySearchResult(
          MemoryDataType::kNationalIdCardNumber, u"123456789",
          {EntryMetadata(MemoryDataType::kNameFull, u"", u"John Doe"),
           EntryMetadata(MemoryDataType::kNationalIdCardCountry, u"", u"USA"),
           EntryMetadata(MemoryDataType::kNationalIdCardExpirationDate, u"",
                         u"2025-03-03"),
           EntryMetadata(MemoryDataType::kNationalIdCardIssueDate, u"",
                         u"2015-03-04"),
           EntryMetadata(MemoryDataType::kNationalIdCardName, u"",
                         u"John Doe")}));

  EXPECT_THAT(
      CreateResultFromEntity(MemoryDataType::kNationalIdCardFull, entity),
      MatchesMemorySearchResult(
          MemoryDataType::kNationalIdCardFull,
          u"John Doe, USA, 123456789, 2015-03-04, 2025-03-03",
          {EntryMetadata(MemoryDataType::kNameFull, u"", u"John Doe"),
           EntryMetadata(MemoryDataType::kNationalIdCardCountry, u"", u"USA"),
           EntryMetadata(MemoryDataType::kNationalIdCardExpirationDate, u"",
                         u"2025-03-03"),
           EntryMetadata(MemoryDataType::kNationalIdCardIssueDate, u"",
                         u"2015-03-04"),
           EntryMetadata(MemoryDataType::kNationalIdCardName, u"", u"John Doe"),
           EntryMetadata(MemoryDataType::kNationalIdCardNumber, u"",
                         u"123456789")}));
}

TEST(MemoryDataTypeUtilTest, CreateResultFromDriversLicenseEntity) {
  DriversLicense dl;
  dl.name = "John Doe";
  dl.number = "D1234567";
  dl.state = "CA";
  dl.issue_date = Date{12, 8, 2019};
  dl.expiration_date = Date{12, 8, 2024};

  Entity entity;
  entity.specifics = dl;

  EXPECT_THAT(
      CreateResultFromEntity(MemoryDataType::kDriversLicenseNumber, entity),
      MatchesMemorySearchResult(
          MemoryDataType::kDriversLicenseNumber, u"D1234567",
          {EntryMetadata(MemoryDataType::kNameFull, u"", u"John Doe"),
           EntryMetadata(MemoryDataType::kDriversLicenseExpirationDate, u"",
                         u"2024-08-12"),
           EntryMetadata(MemoryDataType::kDriversLicenseIssueDate, u"",
                         u"2019-08-12"),
           EntryMetadata(MemoryDataType::kDriversLicenseName, u"", u"John Doe"),
           EntryMetadata(MemoryDataType::kDriversLicenseState, u"", u"CA")}));

  EXPECT_THAT(
      CreateResultFromEntity(MemoryDataType::kDriversLicenseFull, entity),
      MatchesMemorySearchResult(
          MemoryDataType::kDriversLicenseFull,
          u"John Doe, CA, D1234567, 2019-08-12, 2024-08-12",
          {EntryMetadata(MemoryDataType::kNameFull, u"", u"John Doe"),
           EntryMetadata(MemoryDataType::kDriversLicenseExpirationDate, u"",
                         u"2024-08-12"),
           EntryMetadata(MemoryDataType::kDriversLicenseIssueDate, u"",
                         u"2019-08-12"),
           EntryMetadata(MemoryDataType::kDriversLicenseName, u"", u"John Doe"),
           EntryMetadata(MemoryDataType::kDriversLicenseNumber, u"",
                         u"D1234567"),
           EntryMetadata(MemoryDataType::kDriversLicenseState, u"", u"CA")}));
}

TEST(MemoryDataTypeUtilTest, CreateResultFromOrderEntity) {
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

  EXPECT_THAT(
      CreateResultFromEntity(MemoryDataType::kOrderId, entity),
      MatchesMemorySearchResult(
          MemoryDataType::kOrderId, u"ORD12345",
          {EntryMetadata(MemoryDataType::kOrderAccount, u"",
                         u"john.doe@example.com"),
           EntryMetadata(MemoryDataType::kOrderDate, u"", u"2023-11-23"),
           EntryMetadata(MemoryDataType::kOrderGrandTotal, u"", u"$100.00"),
           EntryMetadata(MemoryDataType::kOrderMerchantName, u"", u"Amazon"),
           EntryMetadata(MemoryDataType::kOrderProductNames, u"",
                         u"Laptop, Mouse")}));

  EXPECT_THAT(
      CreateResultFromEntity(MemoryDataType::kOrderFull, entity),
      MatchesMemorySearchResult(
          MemoryDataType::kOrderFull,
          u"ORD12345, john.doe@example.com, Amazon, Laptop, Mouse, $100.00, "
          u"2023-11-23",
          {EntryMetadata(MemoryDataType::kOrderAccount, u"",
                         u"john.doe@example.com"),
           EntryMetadata(MemoryDataType::kOrderDate, u"", u"2023-11-23"),
           EntryMetadata(MemoryDataType::kOrderGrandTotal, u"", u"$100.00"),
           EntryMetadata(MemoryDataType::kOrderId, u"", u"ORD12345"),
           EntryMetadata(MemoryDataType::kOrderMerchantName, u"", u"Amazon"),
           EntryMetadata(MemoryDataType::kOrderProductNames, u"",
                         u"Laptop, Mouse")}));
}

TEST(MemoryDataTypeUtilTest, CreateResultFromOrderEntityWithEmptyAttributes) {
  Order order;
  order.id = "ORD12345";
  order.account = "john.doe@example.com";
  // merchant_name is intentionally empty.
  order.grand_total = "$100.00";

  Entity entity;
  entity.specifics = order;

  EXPECT_THAT(
      CreateResultFromEntity(MemoryDataType::kOrderFull, entity),
      MatchesMemorySearchResult(
          MemoryDataType::kOrderFull,
          u"ORD12345, john.doe@example.com, $100.00",
          {EntryMetadata(MemoryDataType::kOrderAccount, u"",
                         u"john.doe@example.com"),
           EntryMetadata(MemoryDataType::kOrderGrandTotal, u"", u"$100.00"),
           EntryMetadata(MemoryDataType::kOrderId, u"", u"ORD12345")}));

  EXPECT_THAT(
      CreateResultFromEntity(MemoryDataType::kOrderId, entity),
      MatchesMemorySearchResult(
          MemoryDataType::kOrderId, u"ORD12345",
          {EntryMetadata(MemoryDataType::kOrderAccount, u"",
                         u"john.doe@example.com"),
           EntryMetadata(MemoryDataType::kOrderGrandTotal, u"", u"$100.00")}));
}

TEST(MemoryDataTypeUtilTest, CreateResultFromShipmentEntity) {
  Shipment shipment;
  shipment.tracking_number = "1Z9999999999999999";
  shipment.associated_order_id = "ORD123";
  shipment.delivery_address = "123 Main St";
  shipment.carrier_name = "UPS";
  shipment.carrier_domain = GURL("https://www.example.com");
  shipment.shipped_date = Date{15, 6, 2026};

  Entity entity;
  entity.specifics = shipment;

  EXPECT_THAT(
      CreateResultFromEntity(MemoryDataType::kShipmentTrackingNumber, entity),
      MatchesMemorySearchResult(
          MemoryDataType::kShipmentTrackingNumber, u"1Z9999999999999999",
          {EntryMetadata(MemoryDataType::kShipmentAssociatedOrderId, u"",
                         u"ORD123"),
           EntryMetadata(MemoryDataType::kShipmentCarrierDomain, u"",
                         u"https://www.example.com/"),
           EntryMetadata(MemoryDataType::kShipmentCarrierName, u"", u"UPS"),
           EntryMetadata(MemoryDataType::kShipmentDeliveryAddress, u"",
                         u"123 Main St"),
           EntryMetadata(MemoryDataType::kShipmentShippedDate, u"",
                         u"2026-06-15")}));

  EXPECT_THAT(
      CreateResultFromEntity(MemoryDataType::kShipmentFull, entity),
      MatchesMemorySearchResult(
          MemoryDataType::kShipmentFull,
          u"1Z9999999999999999, ORD123, 123 Main St, UPS, "
          u"https://www.example.com/, 2026-06-15",
          {EntryMetadata(MemoryDataType::kShipmentAssociatedOrderId, u"",
                         u"ORD123"),
           EntryMetadata(MemoryDataType::kShipmentCarrierDomain, u"",
                         u"https://www.example.com/"),
           EntryMetadata(MemoryDataType::kShipmentCarrierName, u"", u"UPS"),
           EntryMetadata(MemoryDataType::kShipmentDeliveryAddress, u"",
                         u"123 Main St"),
           EntryMetadata(MemoryDataType::kShipmentShippedDate, u"",
                         u"2026-06-15"),
           EntryMetadata(MemoryDataType::kShipmentTrackingNumber, u"",
                         u"1Z9999999999999999")}));
}

TEST(MemoryDataTypeUtilTest, GetEntityTypesForMemoryDataType) {
  EXPECT_EQ(GetEntityTypesForMemoryDataType(MemoryDataType::kVehicleMake),
            EntityTypeEnumSet({EntityType::kVehicle}));

  EXPECT_EQ(GetEntityTypesForMemoryDataType(MemoryDataType::kPassportName),
            EntityTypeEnumSet({EntityType::kPassport}));

  EXPECT_EQ(GetEntityTypesForMemoryDataType(
                MemoryDataType::kFlightReservationFlightNumber),
            EntityTypeEnumSet({EntityType::kFlightReservation}));

  EXPECT_EQ(
      GetEntityTypesForMemoryDataType(MemoryDataType::kNationalIdCardNumber),
      EntityTypeEnumSet({EntityType::kNationalId}));

  EXPECT_EQ(
      GetEntityTypesForMemoryDataType(MemoryDataType::kDriversLicenseNumber),
      EntityTypeEnumSet({EntityType::kDriversLicense}));

  EXPECT_EQ(GetEntityTypesForMemoryDataType(MemoryDataType::kOrderId),
            EntityTypeEnumSet({EntityType::kOrder}));

  EXPECT_EQ(
      GetEntityTypesForMemoryDataType(MemoryDataType::kShipmentTrackingNumber),
      EntityTypeEnumSet({EntityType::kShipment}));

  EXPECT_EQ(GetEntityTypesForMemoryDataType(MemoryDataType::kNameFull),
            EntityTypeEnumSet(
                {EntityType::kVehicle, EntityType::kPassport,
                 EntityType::kFlightReservation, EntityType::kNationalId,
                 EntityType::kDriversLicense, EntityType::kOrder}));
  EXPECT_EQ(GetEntityTypesForMemoryDataType(MemoryDataType::kUnknown),
            EntityTypeEnumSet({EntityType::kUnknown}));
}

// Tests that `ToPersonalContextEntity` correctly converts individual memory
// entry attributes and metadata into the corresponding fields of the personal
// context Entity proto message.
TEST(MemoryDataTypeUtilTest, ToPersonalContextEntity) {
  // Test Passport conversion.
  {
    std::u16string value = u"P12345";
    MemoryDataType memory_data_type = MemoryDataType::kPassportNumber;

    std::vector<EntryMetadata> metadata_list;
    metadata_list.emplace_back(MemoryDataType::kPassportName, u"Passport Name",
                               u"Jane Doe");
    metadata_list.emplace_back(MemoryDataType::kPassportCountry,
                               u"Passport Country", u"US");
    metadata_list.emplace_back(MemoryDataType::kPassportExpirationDate,
                               u"Passport Expiration Date", u"2030-05-20");

    personal_context::proto::Entity entity =
        ToPersonalContextEntity(value, memory_data_type, metadata_list);

    ASSERT_TRUE(entity.has_passport());
    EXPECT_EQ(entity.passport().number(), "P12345");
    EXPECT_EQ(entity.passport().name(), "Jane Doe");
    EXPECT_EQ(entity.passport().issuing_country(), "US");
    EXPECT_EQ(entity.passport().expiration_date().year(), 2030);
    EXPECT_EQ(entity.passport().expiration_date().month(), 5);
    EXPECT_EQ(entity.passport().expiration_date().day(), 20);
  }

  // Test Order conversion, including order date parsing and product names list.
  {
    std::u16string value = u"ORD-123";
    MemoryDataType memory_data_type = MemoryDataType::kOrderId;

    std::vector<EntryMetadata> metadata_list;
    metadata_list.emplace_back(MemoryDataType::kOrderDate, u"Order Date",
                               u"2026-07-07");
    metadata_list.emplace_back(MemoryDataType::kOrderProductNames, u"Products",
                               u"Book A, Toy B");

    personal_context::proto::Entity entity =
        ToPersonalContextEntity(value, memory_data_type, metadata_list);

    ASSERT_TRUE(entity.has_order());
    EXPECT_EQ(entity.order().order_id(), "ORD-123");
    EXPECT_EQ(entity.order().order_date().year(), 2026);
    EXPECT_EQ(entity.order().order_date().month(), 7);
    EXPECT_EQ(entity.order().order_date().day(), 7);
    ASSERT_EQ(entity.order().product_names_size(), 2);
    EXPECT_EQ(entity.order().product_names(0), "Book A");
    EXPECT_EQ(entity.order().product_names(1), "Toy B");
  }

  // Test Shipment conversion, including associated order IDs parsing and
  // shipped date parsing.
  {
    std::u16string value = u"TRACK-888";
    MemoryDataType memory_data_type = MemoryDataType::kShipmentTrackingNumber;

    std::vector<EntryMetadata> metadata_list;
    metadata_list.emplace_back(MemoryDataType::kShipmentAssociatedOrderId,
                               u"Order IDs", u"ORD-001, ORD-002");
    metadata_list.emplace_back(MemoryDataType::kShipmentShippedDate,
                               u"Ship Date", u"2026-07-07");

    personal_context::proto::Entity entity =
        ToPersonalContextEntity(value, memory_data_type, metadata_list);

    ASSERT_TRUE(entity.has_shipment());
    EXPECT_EQ(entity.shipment().tracking_number(), "TRACK-888");
    ASSERT_EQ(entity.shipment().associated_order_ids_size(), 2);
    EXPECT_EQ(entity.shipment().associated_order_ids(0), "ORD-001");
    EXPECT_EQ(entity.shipment().associated_order_ids(1), "ORD-002");
    EXPECT_EQ(entity.shipment().ship_date().year(), 2026);
    EXPECT_EQ(entity.shipment().ship_date().month(), 7);
    EXPECT_EQ(entity.shipment().ship_date().day(), 7);
  }
}

}  // namespace

}  // namespace accessibility_annotator
