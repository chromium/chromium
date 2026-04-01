// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type_util.h"

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
    QueryIntentType type,
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

TEST(QueryIntentTypeUtilTest, CreateResultFromVehicleEntity) {
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
      CreateResultFromEntity(QueryIntentType::kVehicleMake, entity),
      MatchesMemorySearchResult(
          QueryIntentType::kVehicleMake, u"Toyota",
          {EntryMetadata(QueryIntentType::kNameFull, u"", u"John Doe"),
           EntryMetadata(QueryIntentType::kVehicleModel, u"", u"Camry"),
           EntryMetadata(QueryIntentType::kVehicleOwner, u"", u"John Doe"),
           EntryMetadata(QueryIntentType::kVehiclePlateNumber, u"", u"XYZ123"),
           EntryMetadata(QueryIntentType::kVehiclePlateState, u"", u"CA"),
           EntryMetadata(QueryIntentType::kVehicleVin, u"", u"1234567890VIN"),
           EntryMetadata(QueryIntentType::kVehicleYear, u"", u"2020")}));

  EXPECT_THAT(
      CreateResultFromEntity(QueryIntentType::kVehicle, entity),
      MatchesMemorySearchResult(
          QueryIntentType::kVehicle,
          u"Toyota, Camry, 2020, John Doe, XYZ123, CA, 1234567890VIN",
          {EntryMetadata(QueryIntentType::kNameFull, u"", u"John Doe"),
           EntryMetadata(QueryIntentType::kVehicleMake, u"", u"Toyota"),
           EntryMetadata(QueryIntentType::kVehicleModel, u"", u"Camry"),
           EntryMetadata(QueryIntentType::kVehicleOwner, u"", u"John Doe"),
           EntryMetadata(QueryIntentType::kVehiclePlateNumber, u"", u"XYZ123"),
           EntryMetadata(QueryIntentType::kVehiclePlateState, u"", u"CA"),
           EntryMetadata(QueryIntentType::kVehicleVin, u"", u"1234567890VIN"),
           EntryMetadata(QueryIntentType::kVehicleYear, u"", u"2020")}));
}

TEST(QueryIntentTypeUtilTest,
     CreateResultFromVehicleEntityWithEmptyAttributes) {
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
      CreateResultFromEntity(QueryIntentType::kVehicle, entity),
      MatchesMemorySearchResult(
          QueryIntentType::kVehicle, u"Toyota, 2020, XYZ123, CA, 1234567890VIN",
          {EntryMetadata(QueryIntentType::kVehicleMake, u"", u"Toyota"),
           EntryMetadata(QueryIntentType::kVehiclePlateNumber, u"", u"XYZ123"),
           EntryMetadata(QueryIntentType::kVehiclePlateState, u"", u"CA"),
           EntryMetadata(QueryIntentType::kVehicleVin, u"", u"1234567890VIN"),
           EntryMetadata(QueryIntentType::kVehicleYear, u"", u"2020")}));

  EXPECT_THAT(
      CreateResultFromEntity(QueryIntentType::kVehicleMake, entity),
      MatchesMemorySearchResult(
          QueryIntentType::kVehicleMake, u"Toyota",
          {EntryMetadata(QueryIntentType::kVehiclePlateNumber, u"", u"XYZ123"),
           EntryMetadata(QueryIntentType::kVehiclePlateState, u"", u"CA"),
           EntryMetadata(QueryIntentType::kVehicleVin, u"", u"1234567890VIN"),
           EntryMetadata(QueryIntentType::kVehicleYear, u"", u"2020")}));
}

TEST(QueryIntentTypeUtilTest, CreateResultFromPassportEntity) {
  Passport passport;
  passport.name = "John Doe";
  passport.issuing_country = "USA";
  passport.number = "123456789";
  passport.issue_date = Date{15, 2, 2018};
  passport.expiration_date = Date{31, 10, 2028};

  Entity entity;
  entity.specifics = passport;

  EXPECT_THAT(
      CreateResultFromEntity(QueryIntentType::kPassportName, entity),
      MatchesMemorySearchResult(
          QueryIntentType::kPassportName, u"John Doe",
          {EntryMetadata(QueryIntentType::kNameFull, u"", u"John Doe"),
           EntryMetadata(QueryIntentType::kPassportCountry, u"", u"USA"),
           EntryMetadata(QueryIntentType::kPassportExpirationDate, u"",
                         u"2028-10-31"),
           EntryMetadata(QueryIntentType::kPassportIssueDate, u"",
                         u"2018-02-15"),
           EntryMetadata(QueryIntentType::kPassportNumber, u"",
                         u"123456789")}));

  EXPECT_THAT(
      CreateResultFromEntity(QueryIntentType::kPassportFull, entity),
      MatchesMemorySearchResult(
          QueryIntentType::kPassportFull,
          u"John Doe, USA, 123456789, 2018-02-15, 2028-10-31",
          {EntryMetadata(QueryIntentType::kNameFull, u"", u"John Doe"),
           EntryMetadata(QueryIntentType::kPassportCountry, u"", u"USA"),
           EntryMetadata(QueryIntentType::kPassportExpirationDate, u"",
                         u"2028-10-31"),
           EntryMetadata(QueryIntentType::kPassportIssueDate, u"",
                         u"2018-02-15"),
           EntryMetadata(QueryIntentType::kPassportName, u"", u"John Doe"),
           EntryMetadata(QueryIntentType::kPassportNumber, u"",
                         u"123456789")}));
}

TEST(QueryIntentTypeUtilTest,
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
      CreateResultFromEntity(QueryIntentType::kPassportFull, entity),
      MatchesMemorySearchResult(
          QueryIntentType::kPassportFull, u"John Doe, 123456789, 2025-06-20",
          {EntryMetadata(QueryIntentType::kNameFull, u"", u"John Doe"),
           EntryMetadata(QueryIntentType::kPassportExpirationDate, u"",
                         u"2025-06-20"),
           EntryMetadata(QueryIntentType::kPassportName, u"", u"John Doe"),
           EntryMetadata(QueryIntentType::kPassportNumber, u"",
                         u"123456789")}));

  EXPECT_THAT(CreateResultFromEntity(QueryIntentType::kPassportName, entity),
              MatchesMemorySearchResult(
                  QueryIntentType::kPassportName, u"John Doe",
                  {EntryMetadata(QueryIntentType::kNameFull, u"", u"John Doe"),
                   EntryMetadata(QueryIntentType::kPassportExpirationDate, u"",
                                 u"2025-06-20"),
                   EntryMetadata(QueryIntentType::kPassportNumber, u"",
                                 u"123456789")}));
}

TEST(QueryIntentTypeUtilTest, CreateResultFromFlightReservationEntity) {
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
      CreateResultFromEntity(QueryIntentType::kFlightReservationFlightNumber,
                             entity),
      MatchesMemorySearchResult(
          QueryIntentType::kFlightReservationFlightNumber, u"DL123",
          {EntryMetadata(QueryIntentType::kNameFull, u"", u"John Doe"),
           EntryMetadata(QueryIntentType::kFlightReservationArrivalAirport, u"",
                         u"LAX"),
           EntryMetadata(QueryIntentType::kFlightReservationConfirmationCode,
                         u"", u"ABCDEF"),
           EntryMetadata(QueryIntentType::kFlightReservationDepartureAirport,
                         u"", u"JFK"),
           EntryMetadata(QueryIntentType::kFlightReservationDepartureDate, u"",
                         u"2026-04-20"),
           EntryMetadata(QueryIntentType::kFlightReservationPassengerName, u"",
                         u"John Doe"),
           EntryMetadata(QueryIntentType::kFlightReservationTicketNumber, u"",
                         u"0061234567890")}));

  EXPECT_THAT(
      CreateResultFromEntity(QueryIntentType::kFlightReservationFull, entity),
      MatchesMemorySearchResult(
          QueryIntentType::kFlightReservationFull,
          u"DL123, 0061234567890, ABCDEF, John Doe, JFK, LAX, 2026-04-20",
          {EntryMetadata(QueryIntentType::kNameFull, u"", u"John Doe"),
           EntryMetadata(QueryIntentType::kFlightReservationArrivalAirport, u"",
                         u"LAX"),
           EntryMetadata(QueryIntentType::kFlightReservationConfirmationCode,
                         u"", u"ABCDEF"),
           EntryMetadata(QueryIntentType::kFlightReservationDepartureAirport,
                         u"", u"JFK"),
           EntryMetadata(QueryIntentType::kFlightReservationDepartureDate, u"",
                         u"2026-04-20"),
           EntryMetadata(QueryIntentType::kFlightReservationFlightNumber, u"",
                         u"DL123"),
           EntryMetadata(QueryIntentType::kFlightReservationPassengerName, u"",
                         u"John Doe"),
           EntryMetadata(QueryIntentType::kFlightReservationTicketNumber, u"",
                         u"0061234567890")}));
}

TEST(QueryIntentTypeUtilTest, CreateResultFromNationalIdEntity) {
  NationalId national_id;
  national_id.name = "John Doe";
  national_id.number = "123456789";
  national_id.issuing_country = "USA";
  national_id.issue_date = Date{4, 3, 2015};
  national_id.expiration_date = Date{3, 3, 2025};

  Entity entity;
  entity.specifics = national_id;

  EXPECT_THAT(
      CreateResultFromEntity(QueryIntentType::kNationalIdCardNumber, entity),
      MatchesMemorySearchResult(
          QueryIntentType::kNationalIdCardNumber, u"123456789",
          {EntryMetadata(QueryIntentType::kNameFull, u"", u"John Doe"),
           EntryMetadata(QueryIntentType::kNationalIdCardCountry, u"", u"USA"),
           EntryMetadata(QueryIntentType::kNationalIdCardExpirationDate, u"",
                         u"2025-03-03"),
           EntryMetadata(QueryIntentType::kNationalIdCardIssueDate, u"",
                         u"2015-03-04"),
           EntryMetadata(QueryIntentType::kNationalIdCardName, u"",
                         u"John Doe")}));

  EXPECT_THAT(
      CreateResultFromEntity(QueryIntentType::kNationalIdCardFull, entity),
      MatchesMemorySearchResult(
          QueryIntentType::kNationalIdCardFull,
          u"John Doe, USA, 123456789, 2015-03-04, 2025-03-03",
          {EntryMetadata(QueryIntentType::kNameFull, u"", u"John Doe"),
           EntryMetadata(QueryIntentType::kNationalIdCardCountry, u"", u"USA"),
           EntryMetadata(QueryIntentType::kNationalIdCardExpirationDate, u"",
                         u"2025-03-03"),
           EntryMetadata(QueryIntentType::kNationalIdCardIssueDate, u"",
                         u"2015-03-04"),
           EntryMetadata(QueryIntentType::kNationalIdCardName, u"",
                         u"John Doe"),
           EntryMetadata(QueryIntentType::kNationalIdCardNumber, u"",
                         u"123456789")}));
}

TEST(QueryIntentTypeUtilTest, CreateResultFromDriversLicenseEntity) {
  DriversLicense dl;
  dl.name = "John Doe";
  dl.number = "D1234567";
  dl.state = "CA";
  dl.issue_date = Date{12, 8, 2019};
  dl.expiration_date = Date{12, 8, 2024};

  Entity entity;
  entity.specifics = dl;

  EXPECT_THAT(
      CreateResultFromEntity(QueryIntentType::kDriversLicenseNumber, entity),
      MatchesMemorySearchResult(
          QueryIntentType::kDriversLicenseNumber, u"D1234567",
          {EntryMetadata(QueryIntentType::kNameFull, u"", u"John Doe"),
           EntryMetadata(QueryIntentType::kDriversLicenseExpirationDate, u"",
                         u"2024-08-12"),
           EntryMetadata(QueryIntentType::kDriversLicenseIssueDate, u"",
                         u"2019-08-12"),
           EntryMetadata(QueryIntentType::kDriversLicenseName, u"",
                         u"John Doe"),
           EntryMetadata(QueryIntentType::kDriversLicenseState, u"", u"CA")}));

  EXPECT_THAT(
      CreateResultFromEntity(QueryIntentType::kDriversLicenseFull, entity),
      MatchesMemorySearchResult(
          QueryIntentType::kDriversLicenseFull,
          u"John Doe, CA, D1234567, 2019-08-12, 2024-08-12",
          {EntryMetadata(QueryIntentType::kNameFull, u"", u"John Doe"),
           EntryMetadata(QueryIntentType::kDriversLicenseExpirationDate, u"",
                         u"2024-08-12"),
           EntryMetadata(QueryIntentType::kDriversLicenseIssueDate, u"",
                         u"2019-08-12"),
           EntryMetadata(QueryIntentType::kDriversLicenseName, u"",
                         u"John Doe"),
           EntryMetadata(QueryIntentType::kDriversLicenseNumber, u"",
                         u"D1234567"),
           EntryMetadata(QueryIntentType::kDriversLicenseState, u"", u"CA")}));
}

TEST(QueryIntentTypeUtilTest, CreateResultFromOrderEntity) {
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
      CreateResultFromEntity(QueryIntentType::kOrderId, entity),
      MatchesMemorySearchResult(
          QueryIntentType::kOrderId, u"ORD12345",
          {EntryMetadata(QueryIntentType::kOrderAccount, u"",
                         u"john.doe@example.com"),
           EntryMetadata(QueryIntentType::kOrderDate, u"", u"2023-11-23"),
           EntryMetadata(QueryIntentType::kOrderGrandTotal, u"", u"$100.00"),
           EntryMetadata(QueryIntentType::kOrderMerchantName, u"", u"Amazon"),
           EntryMetadata(QueryIntentType::kOrderProductNames, u"",
                         u"Laptop, Mouse")}));

  EXPECT_THAT(
      CreateResultFromEntity(QueryIntentType::kOrderFull, entity),
      MatchesMemorySearchResult(
          QueryIntentType::kOrderFull,
          u"ORD12345, john.doe@example.com, Amazon, Laptop, Mouse, $100.00, "
          u"2023-11-23",
          {EntryMetadata(QueryIntentType::kOrderAccount, u"",
                         u"john.doe@example.com"),
           EntryMetadata(QueryIntentType::kOrderDate, u"", u"2023-11-23"),
           EntryMetadata(QueryIntentType::kOrderGrandTotal, u"", u"$100.00"),
           EntryMetadata(QueryIntentType::kOrderId, u"", u"ORD12345"),
           EntryMetadata(QueryIntentType::kOrderMerchantName, u"", u"Amazon"),
           EntryMetadata(QueryIntentType::kOrderProductNames, u"",
                         u"Laptop, Mouse")}));
}

TEST(QueryIntentTypeUtilTest, CreateResultFromOrderEntityWithEmptyAttributes) {
  Order order;
  order.id = "ORD12345";
  order.account = "john.doe@example.com";
  // merchant_name is intentionally empty.
  order.grand_total = "$100.00";

  Entity entity;
  entity.specifics = order;

  EXPECT_THAT(
      CreateResultFromEntity(QueryIntentType::kOrderFull, entity),
      MatchesMemorySearchResult(
          QueryIntentType::kOrderFull,
          u"ORD12345, john.doe@example.com, $100.00",
          {EntryMetadata(QueryIntentType::kOrderAccount, u"",
                         u"john.doe@example.com"),
           EntryMetadata(QueryIntentType::kOrderGrandTotal, u"", u"$100.00"),
           EntryMetadata(QueryIntentType::kOrderId, u"", u"ORD12345")}));

  EXPECT_THAT(
      CreateResultFromEntity(QueryIntentType::kOrderId, entity),
      MatchesMemorySearchResult(
          QueryIntentType::kOrderId, u"ORD12345",
          {EntryMetadata(QueryIntentType::kOrderAccount, u"",
                         u"john.doe@example.com"),
           EntryMetadata(QueryIntentType::kOrderGrandTotal, u"", u"$100.00")}));
}

TEST(QueryIntentTypeUtilTest, CreateResultFromShipmentEntity) {
  Shipment shipment;
  shipment.tracking_number = "1Z9999999999999999";
  shipment.associated_order_id = "ORD123";
  shipment.delivery_zip_code = "80339";
  shipment.carrier_name = "UPS";
  shipment.carrier_domain = GURL("https://www.example.com");
  shipment.estimated_delivery_date = Date{15, 6, 2026};

  Entity entity;
  entity.specifics = shipment;

  EXPECT_THAT(
      CreateResultFromEntity(QueryIntentType::kShipmentTrackingNumber, entity),
      MatchesMemorySearchResult(
          QueryIntentType::kShipmentTrackingNumber, u"1Z9999999999999999",
          {EntryMetadata(QueryIntentType::kShipmentAssociatedOrderId, u"",
                         u"ORD123"),
           EntryMetadata(QueryIntentType::kShipmentCarrierDomain, u"",
                         u"https://www.example.com/"),
           EntryMetadata(QueryIntentType::kShipmentCarrierName, u"", u"UPS"),
           EntryMetadata(QueryIntentType::kShipmentDeliveryZipCode, u"",
                         u"80339"),
           EntryMetadata(QueryIntentType::kShipmentEstimatedDeliveryDate, u"",
                         u"2026-06-15")}));

  EXPECT_THAT(
      CreateResultFromEntity(QueryIntentType::kShipmentFull, entity),
      MatchesMemorySearchResult(
          QueryIntentType::kShipmentFull,
          u"1Z9999999999999999, ORD123, 80339, UPS, "
          u"https://www.example.com/, 2026-06-15",
          {EntryMetadata(QueryIntentType::kShipmentAssociatedOrderId, u"",
                         u"ORD123"),
           EntryMetadata(QueryIntentType::kShipmentCarrierDomain, u"",
                         u"https://www.example.com/"),
           EntryMetadata(QueryIntentType::kShipmentCarrierName, u"", u"UPS"),
           EntryMetadata(QueryIntentType::kShipmentDeliveryZipCode, u"",
                         u"80339"),
           EntryMetadata(QueryIntentType::kShipmentEstimatedDeliveryDate, u"",
                         u"2026-06-15"),
           EntryMetadata(QueryIntentType::kShipmentTrackingNumber, u"",
                         u"1Z9999999999999999")}));
}

TEST(QueryIntentTypeUtilTest, GetEntityTypesForQueryIntentType) {
  EXPECT_EQ(GetEntityTypesForQueryIntentType(QueryIntentType::kVehicleMake),
            EntityTypeEnumSet({EntityType::kVehicle}));

  EXPECT_EQ(GetEntityTypesForQueryIntentType(QueryIntentType::kPassportName),
            EntityTypeEnumSet({EntityType::kPassport}));

  EXPECT_EQ(GetEntityTypesForQueryIntentType(
                QueryIntentType::kFlightReservationFlightNumber),
            EntityTypeEnumSet({EntityType::kFlightReservation}));

  EXPECT_EQ(
      GetEntityTypesForQueryIntentType(QueryIntentType::kNationalIdCardNumber),
      EntityTypeEnumSet({EntityType::kNationalId}));

  EXPECT_EQ(
      GetEntityTypesForQueryIntentType(QueryIntentType::kDriversLicenseNumber),
      EntityTypeEnumSet({EntityType::kDriversLicense}));

  EXPECT_EQ(GetEntityTypesForQueryIntentType(QueryIntentType::kOrderId),
            EntityTypeEnumSet({EntityType::kOrder}));

  EXPECT_EQ(GetEntityTypesForQueryIntentType(
                QueryIntentType::kShipmentTrackingNumber),
            EntityTypeEnumSet({EntityType::kShipment}));

  EXPECT_EQ(GetEntityTypesForQueryIntentType(QueryIntentType::kNameFull),
            EntityTypeEnumSet(
                {EntityType::kVehicle, EntityType::kPassport,
                 EntityType::kFlightReservation, EntityType::kNationalId,
                 EntityType::kDriversLicense, EntityType::kOrder}));
  EXPECT_EQ(GetEntityTypesForQueryIntentType(QueryIntentType::kUnknown),
            EntityTypeEnumSet({EntityType::kUnknown}));
}

}  // namespace

}  // namespace accessibility_annotator
