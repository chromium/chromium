// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/personal_context_conversion_util.h"

#include <optional>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance_test_api.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/personal_context/proto/features/common_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using enum AttributeTypeName;
using Source = EntityInstance::PersonalContextRecordTypePayload::Source;

// Helper to check the string value of an attribute.
void ExpectAttributeValue(const EntityInstance& entity,
                          AttributeTypeName attribute_name,
                          const std::u16string& expected_value) {
  base::optional_ref<const AttributeInstance> attr =
      entity.attribute(AttributeType(attribute_name));
  ASSERT_TRUE(attr.has_value())
      << "Missing attribute " << static_cast<int>(attribute_name);
  EXPECT_EQ(attr->GetCompleteRawInfo(), expected_value);
  EXPECT_FALSE(attr->masked());
}

void ExpectMaskedAttributeValue(const EntityInstance& entity,
                                AttributeTypeName attribute_name,
                                const std::u16string& expected_value) {
  base::optional_ref<const AttributeInstance> attr =
      entity.attribute(AttributeType(attribute_name));
  ASSERT_TRUE(attr.has_value())
      << "Missing attribute " << static_cast<int>(attribute_name);
  EXPECT_EQ(attr->GetCompleteRawInfo(), expected_value);
  EXPECT_TRUE(attr->masked());
}

TEST(AutofillAiPersonalContextConverters, ConvertPassport) {
  personal_context::proto::Passport passport;
  passport.set_name("Jane Doe");
  passport.set_number("P12345");
  passport.set_issuing_country("US");
  passport.mutable_expiration_date()->set_year(2030);
  passport.mutable_expiration_date()->set_month(5);
  passport.mutable_expiration_date()->set_day(20);
  passport.mutable_issue_date()->set_year(2020);
  passport.mutable_issue_date()->set_month(5);
  passport.mutable_issue_date()->set_day(20);

  personal_context::proto::Entity entity;
  *entity.mutable_passport() = passport;

  std::optional<EntityInstance> opt_result =
      PersonalContextEntityToEntityInstance(entity);

  ASSERT_TRUE(opt_result.has_value());
  const EntityInstance& result = opt_result.value();

  EXPECT_EQ(result.type().name(), EntityTypeName::kPassport);
  EXPECT_EQ(result.record_type(), EntityInstance::RecordType::kPersonalContext);
  EXPECT_TRUE(result.are_attributes_read_only().value());

  ExpectAttributeValue(result, kPassportName, u"Jane Doe");
  ExpectMaskedAttributeValue(result, kPassportNumber, u"P12345");
  ExpectAttributeValue(result, kPassportCountry, u"US");
  ExpectAttributeValue(result, kPassportExpirationDate, u"2030-05-20");
  ExpectAttributeValue(result, kPassportIssueDate, u"2020-05-20");
}

TEST(AutofillAiPersonalContextConverters, ConvertPassport_Unmasked) {
  personal_context::proto::Passport passport;
  passport.set_name("Jane Doe");
  passport.set_number("P12345");
  passport.set_issuing_country("US");

  personal_context::proto::Entity entity;
  *entity.mutable_passport() = passport;

  std::optional<EntityInstance> opt_result =
      PersonalContextEntityToEntityInstance(entity, /*is_masked=*/false);

  ASSERT_TRUE(opt_result.has_value());
  const EntityInstance& result = opt_result.value();

  EXPECT_EQ(result.type().name(), EntityTypeName::kPassport);
  ExpectAttributeValue(result, kPassportName, u"Jane Doe");
  ExpectAttributeValue(result, kPassportNumber, u"P12345");
  ExpectAttributeValue(result, kPassportCountry, u"US");
}

TEST(AutofillAiPersonalContextConverters, ConvertDriversLicense) {
  personal_context::proto::DriversLicense dl;
  dl.set_name("John Smith");
  dl.set_number("DL9876");
  dl.set_state("CA");
  dl.mutable_expiration_date()->set_year(2028);
  dl.mutable_expiration_date()->set_month(12);
  dl.mutable_expiration_date()->set_day(31);
  dl.mutable_issue_date()->set_year(2018);
  dl.mutable_issue_date()->set_month(1);
  dl.mutable_issue_date()->set_day(1);

  personal_context::proto::Entity entity;
  *entity.mutable_drivers_license() = dl;

  std::optional<EntityInstance> opt_result =
      PersonalContextEntityToEntityInstance(entity);

  ASSERT_TRUE(opt_result.has_value());
  const EntityInstance& result = opt_result.value();

  EXPECT_EQ(result.type().name(), EntityTypeName::kDriversLicense);

  ExpectAttributeValue(result, kDriversLicenseName, u"John Smith");
  ExpectMaskedAttributeValue(result, kDriversLicenseNumber, u"DL9876");
  ExpectAttributeValue(result, kDriversLicenseState, u"CA");
  ExpectAttributeValue(result, kDriversLicenseExpirationDate, u"2028-12-31");
  ExpectAttributeValue(result, kDriversLicenseIssueDate, u"2018-01-01");
}

TEST(AutofillAiPersonalContextConverters, ConvertDriversLicense_Unmasked) {
  personal_context::proto::DriversLicense dl;
  dl.set_name("John Smith");
  dl.set_number("DL9876");
  dl.set_state("CA");

  personal_context::proto::Entity entity;
  *entity.mutable_drivers_license() = dl;

  std::optional<EntityInstance> opt_result =
      PersonalContextEntityToEntityInstance(entity, /*is_masked=*/false);

  ASSERT_TRUE(opt_result.has_value());
  const EntityInstance& result = opt_result.value();

  EXPECT_EQ(result.type().name(), EntityTypeName::kDriversLicense);
  ExpectAttributeValue(result, kDriversLicenseName, u"John Smith");
  ExpectAttributeValue(result, kDriversLicenseNumber, u"DL9876");
  ExpectAttributeValue(result, kDriversLicenseState, u"CA");
}

TEST(AutofillAiPersonalContextConverters, ConvertNationalIdCard) {
  personal_context::proto::NationalId nid;
  nid.set_name("Alice Green");
  nid.set_number("NID5432");
  nid.set_issuing_country("DE");
  nid.mutable_expiration_date()->set_year(2029);
  nid.mutable_expiration_date()->set_month(6);
  nid.mutable_expiration_date()->set_day(15);
  nid.mutable_issue_date()->set_year(2019);
  nid.mutable_issue_date()->set_month(6);
  nid.mutable_issue_date()->set_day(15);

  personal_context::proto::Entity entity;
  *entity.mutable_national_id() = nid;

  std::optional<EntityInstance> opt_result =
      PersonalContextEntityToEntityInstance(entity);

  ASSERT_TRUE(opt_result.has_value());
  const EntityInstance& result = opt_result.value();

  EXPECT_EQ(result.type().name(), EntityTypeName::kNationalIdCard);

  ExpectAttributeValue(result, kNationalIdCardName, u"Alice Green");
  ExpectMaskedAttributeValue(result, kNationalIdCardNumber, u"NID5432");
  ExpectAttributeValue(result, kNationalIdCardCountry, u"DE");
  ExpectAttributeValue(result, kNationalIdCardExpirationDate, u"2029-06-15");
  ExpectAttributeValue(result, kNationalIdCardIssueDate, u"2019-06-15");
}

TEST(AutofillAiPersonalContextConverters, ConvertNationalIdCard_Unmasked) {
  personal_context::proto::NationalId nid;
  nid.set_name("Alice Green");
  nid.set_number("NID5432");
  nid.set_issuing_country("DE");

  personal_context::proto::Entity entity;
  *entity.mutable_national_id() = nid;

  std::optional<EntityInstance> opt_result =
      PersonalContextEntityToEntityInstance(entity, /*is_masked=*/false);

  ASSERT_TRUE(opt_result.has_value());
  const EntityInstance& result = opt_result.value();

  EXPECT_EQ(result.type().name(), EntityTypeName::kNationalIdCard);
  ExpectAttributeValue(result, kNationalIdCardName, u"Alice Green");
  ExpectAttributeValue(result, kNationalIdCardNumber, u"NID5432");
  ExpectAttributeValue(result, kNationalIdCardCountry, u"DE");
}

TEST(AutofillAiPersonalContextConverters, ConvertFlightReservation) {
  personal_context::proto::FlightReservation flight;
  flight.set_flight_number("LH456");
  flight.set_flight_ticket_number("TKT789");
  flight.set_flight_confirmation_code("CONF321");
  flight.set_passenger_name("Bob Dylan");
  flight.set_departure_airport("FRA");
  flight.set_arrival_airport("JFK");
  flight.mutable_departure_time()->set_year(2026);
  flight.mutable_departure_time()->set_month(5);
  flight.mutable_departure_time()->set_day(28);
  flight.mutable_departure_time()->set_hours(12);

  personal_context::proto::Entity entity;
  *entity.mutable_flight_reservation() = flight;

  std::optional<EntityInstance> opt_result =
      PersonalContextEntityToEntityInstance(entity);

  ASSERT_TRUE(opt_result.has_value());
  const EntityInstance& result = opt_result.value();

  EXPECT_EQ(result.type().name(), EntityTypeName::kFlightReservation);
  EXPECT_EQ(test_api(result).frecency_override(), "2026-05-28T12:00:00.000Z");

  ExpectAttributeValue(result, kFlightReservationFlightNumber, u"LH456");
  ExpectAttributeValue(result, kFlightReservationTicketNumber, u"TKT789");
  ExpectAttributeValue(result, kFlightReservationConfirmationCode, u"CONF321");
  ExpectAttributeValue(result, kFlightReservationPassengerName, u"Bob Dylan");
  ExpectAttributeValue(result, kFlightReservationDepartureAirport, u"FRA");
  ExpectAttributeValue(result, kFlightReservationArrivalAirport, u"JFK");
  ExpectAttributeValue(result, kFlightReservationDepartureDate, u"2026-05-28");
}

TEST(AutofillAiPersonalContextConverters, ConvertFlightReservationWithOffset) {
  personal_context::proto::FlightReservation flight;
  flight.set_flight_number("LH456");
  flight.set_departure_airport("FRA");
  flight.set_arrival_airport("JFK");
  // 2026-05-28 12:00:00.
  flight.mutable_departure_time()->set_year(2026);
  flight.mutable_departure_time()->set_month(5);
  flight.mutable_departure_time()->set_day(28);
  flight.mutable_departure_time()->set_hours(12);
  // UTC-4 (offset -14400s). Absolute time is 16:00:00 UTC.
  flight.mutable_departure_time()->mutable_utc_offset()->set_seconds(-14400);

  personal_context::proto::Entity entity;
  *entity.mutable_flight_reservation() = flight;

  std::optional<EntityInstance> opt_result =
      PersonalContextEntityToEntityInstance(entity);

  ASSERT_TRUE(opt_result.has_value());
  const EntityInstance& result = opt_result.value();

  EXPECT_EQ(result.type().name(), EntityTypeName::kFlightReservation);
  EXPECT_EQ(test_api(result).frecency_override(), "2026-05-28T16:00:00.000Z");

  ExpectAttributeValue(result, kFlightReservationDepartureDate, u"2026-05-28");
}

TEST(AutofillAiPersonalContextConverters, ConvertVehicle) {
  personal_context::proto::Vehicle vehicle;
  vehicle.set_vehicle_make("Tesla");
  vehicle.set_vehicle_model("Model 3");
  vehicle.set_vehicle_year("2023");
  vehicle.set_vehicle_identification_number("VIN555");
  vehicle.set_vehicle_license_plate("XYZ-789");
  vehicle.set_license_plate_region("WA");
  vehicle.set_owner_name("Elon Musk");

  personal_context::proto::Entity entity;
  *entity.mutable_vehicle() = vehicle;

  std::optional<EntityInstance> opt_result =
      PersonalContextEntityToEntityInstance(entity);

  ASSERT_TRUE(opt_result.has_value());
  const EntityInstance& result = opt_result.value();

  EXPECT_EQ(result.type().name(), EntityTypeName::kVehicle);

  ExpectAttributeValue(result, kVehicleMake, u"Tesla");
  ExpectAttributeValue(result, kVehicleModel, u"Model 3");
  ExpectAttributeValue(result, kVehicleYear, u"2023");
  ExpectAttributeValue(result, kVehicleVin, u"VIN555");
  ExpectAttributeValue(result, kVehiclePlateNumber, u"XYZ-789");
  ExpectAttributeValue(result, kVehiclePlateState, u"WA");
  ExpectAttributeValue(result, kVehicleOwner, u"Elon Musk");
}

TEST(AutofillAiPersonalContextConverters, ConvertOrder) {
  personal_context::proto::Order order;
  order.set_order_id("ORD-001");
  order.set_account("user@test.com");
  order.set_merchant_name("E-Shop");
  order.set_merchant_domain("eshop.com");
  order.mutable_order_date()->set_year(2026);
  order.mutable_order_date()->set_month(5);
  order.mutable_order_date()->set_day(28);
  order.add_product_names("Book A");
  order.add_product_names("Toy B");

  personal_context::proto::Entity entity;
  *entity.mutable_order() = order;

  std::optional<EntityInstance> opt_result =
      PersonalContextEntityToEntityInstance(entity);

  ASSERT_TRUE(opt_result.has_value());
  const EntityInstance& result = opt_result.value();

  EXPECT_EQ(result.type().name(), EntityTypeName::kOrder);

  ExpectAttributeValue(result, kOrderId, u"ORD-001");
  ExpectAttributeValue(result, kOrderAccount, u"user@test.com");
  ExpectAttributeValue(result, kOrderMerchantName, u"E-Shop");
  ExpectAttributeValue(result, kOrderMerchantDomain, u"eshop.com");
  ExpectAttributeValue(result, kOrderDate, u"2026-05-28");
  ExpectAttributeValue(result, kOrderProductNames, u"Book A, Toy B");
}

TEST(AutofillAiPersonalContextConverters, ConvertShipment) {
  personal_context::proto::Shipment shipment;
  shipment.set_tracking_number("TRACK-888");
  shipment.set_carrier_name("ExpressMail");
  shipment.set_carrier_domain("expressmail.com");
  shipment.mutable_ship_date()->set_year(2026);
  shipment.mutable_ship_date()->set_month(5);
  shipment.mutable_ship_date()->set_day(28);
  shipment.set_delivery_zip_code("94043");
  shipment.set_merchant_name("SuperStore");
  shipment.add_product_names("Widget A");
  shipment.add_product_names("Widget B");

  personal_context::proto::Entity entity;
  *entity.mutable_shipment() = shipment;

  std::optional<EntityInstance> opt_result =
      PersonalContextEntityToEntityInstance(entity);

  ASSERT_TRUE(opt_result.has_value());
  const EntityInstance& result = opt_result.value();

  EXPECT_EQ(result.type().name(), EntityTypeName::kShipment);

  ExpectAttributeValue(result, kShipmentTrackingNumber, u"TRACK-888");
  ExpectAttributeValue(result, kShipmentCarrierName, u"ExpressMail");
  ExpectAttributeValue(result, kShipmentCarrierDomain, u"expressmail.com");
  ExpectAttributeValue(result, kShipmentShippedDate, u"2026-05-28");
  ExpectAttributeValue(result, kShipmentDeliveryZipCode, u"94043");
  ExpectAttributeValue(result, kShipmentMerchantName, u"SuperStore");
  ExpectAttributeValue(result, kShipmentProductNames, u"Widget A, Widget B");
}

TEST(AutofillAiPersonalContextConverters, ConvertKnownTravelerNumber) {
  personal_context::proto::KnownTravelerNumber ktn;
  ktn.set_name("Charlie Brown");
  ktn.set_number("KTN12345");

  personal_context::proto::Entity entity;
  *entity.mutable_known_traveler_number() = ktn;

  std::optional<EntityInstance> opt_result =
      PersonalContextEntityToEntityInstance(entity);

  ASSERT_TRUE(opt_result.has_value());
  const EntityInstance& result = opt_result.value();

  EXPECT_EQ(result.type().name(), EntityTypeName::kKnownTravelerNumber);
  ExpectAttributeValue(result, kKnownTravelerNumberName, u"Charlie Brown");
  ExpectMaskedAttributeValue(result, kKnownTravelerNumberNumber, u"KTN12345");
}

TEST(AutofillAiPersonalContextConverters, ConvertKnownTravelerNumber_Unmasked) {
  personal_context::proto::KnownTravelerNumber ktn;
  ktn.set_name("Charlie Brown");
  ktn.set_number("KTN12345");

  personal_context::proto::Entity entity;
  *entity.mutable_known_traveler_number() = ktn;

  std::optional<EntityInstance> opt_result =
      PersonalContextEntityToEntityInstance(entity, /*is_masked=*/false);

  ASSERT_TRUE(opt_result.has_value());
  const EntityInstance& result = opt_result.value();

  EXPECT_EQ(result.type().name(), EntityTypeName::kKnownTravelerNumber);
  ExpectAttributeValue(result, kKnownTravelerNumberName, u"Charlie Brown");
  ExpectAttributeValue(result, kKnownTravelerNumberNumber, u"KTN12345");
}

TEST(AutofillAiPersonalContextConverters, ConvertEntityWithGmailSource) {
  personal_context::proto::Passport passport;
  passport.set_name("Jane Doe");

  personal_context::proto::Entity entity;
  *entity.mutable_passport() = passport;
  personal_context::proto::SourceReference* source =
      entity.add_source_references();
  source->mutable_gmail()->set_message_url(
      "https://mail.google.com/mail/u/0/#inbox/123");
  source->mutable_gmail()->set_subject("Passport Information");

  std::optional<EntityInstance> opt_result =
      PersonalContextEntityToEntityInstance(entity);

  ASSERT_TRUE(opt_result.has_value());
  const EntityInstance& result = opt_result.value();
  EntityInstance::PersonalContextRecordTypePayload payload{
      .sources = {
          Source{.type = Source::Type::kGmail,
                 .url = "https://mail.google.com/mail/u/0/#inbox/123"}}};
  EXPECT_EQ(std::get<EntityInstance::PersonalContextRecordTypePayload>(
                result.record_type_data()),
            payload);
}

TEST(AutofillAiPersonalContextConverters, ConvertEntityWithPhotosSource) {
  personal_context::proto::DriversLicense dl;
  dl.set_name("John Smith");

  personal_context::proto::Entity entity;
  *entity.mutable_drivers_license() = dl;
  personal_context::proto::SourceReference* source =
      entity.add_source_references();
  source->mutable_photos()->set_photos_url(
      "https://photos.google.com/photo/abc");

  std::optional<EntityInstance> opt_result =
      PersonalContextEntityToEntityInstance(entity);

  ASSERT_TRUE(opt_result.has_value());
  const EntityInstance& result = opt_result.value();
  EntityInstance::PersonalContextRecordTypePayload payload{
      .sources = {Source{.type = Source::Type::kPhotos,
                         .url = "https://photos.google.com/photo/abc"}}};
  EXPECT_EQ(std::get<EntityInstance::PersonalContextRecordTypePayload>(
                result.record_type_data()),
            payload);
}

TEST(AutofillAiPersonalContextConverters, ConvertEntityWithMultipleSources) {
  personal_context::proto::Order order;
  order.set_order_id("ORD-001");

  personal_context::proto::Entity entity;
  *entity.mutable_order() = order;

  personal_context::proto::SourceReference* gmail_source =
      entity.add_source_references();
  gmail_source->mutable_gmail()->set_message_url(
      "https://mail.google.com/mail/u/0/#inbox/123");

  personal_context::proto::SourceReference* photos_source =
      entity.add_source_references();
  photos_source->mutable_photos()->set_photos_url(
      "https://photos.google.com/photo/abc");

  // Add empty source.
  entity.add_source_references();

  std::optional<EntityInstance> opt_result =
      PersonalContextEntityToEntityInstance(entity);

  ASSERT_TRUE(opt_result.has_value());
  const EntityInstance& result = opt_result.value();
  EntityInstance::PersonalContextRecordTypePayload payload{
      .sources = {Source{.type = Source::Type::kGmail,
                         .url = "https://mail.google.com/mail/u/0/#inbox/123"},
                  Source{.type = Source::Type::kPhotos,
                         .url = "https://photos.google.com/photo/abc"}}};
  EXPECT_EQ(std::get<EntityInstance::PersonalContextRecordTypePayload>(
                result.record_type_data()),
            payload);
}

TEST(AutofillAiPersonalContextConverters,
     AutofillEntityTypeToPersonalContextEntityType) {
  using personal_context::proto::EntityType;
  using enum EntityTypeName;

  EXPECT_EQ(AutofillEntityTypeToPersonalContextEntityType(
                autofill::EntityType(kOrder)),
            EntityType::ORDER);
  EXPECT_EQ(AutofillEntityTypeToPersonalContextEntityType(
                autofill::EntityType(kShipment)),
            EntityType::SHIPMENT);
  EXPECT_EQ(AutofillEntityTypeToPersonalContextEntityType(
                autofill::EntityType(kDriversLicense)),
            EntityType::DRIVERS_LICENSE);
  EXPECT_EQ(AutofillEntityTypeToPersonalContextEntityType(
                autofill::EntityType(kPassport)),
            EntityType::PASSPORT);
  EXPECT_EQ(AutofillEntityTypeToPersonalContextEntityType(
                autofill::EntityType(kNationalIdCard)),
            EntityType::NATIONAL_ID);
  EXPECT_EQ(AutofillEntityTypeToPersonalContextEntityType(
                autofill::EntityType(kFlightReservation)),
            EntityType::FLIGHT_RESERVATION);
  EXPECT_EQ(AutofillEntityTypeToPersonalContextEntityType(
                autofill::EntityType(kVehicle)),
            EntityType::VEHICLE);
  EXPECT_EQ(AutofillEntityTypeToPersonalContextEntityType(
                autofill::EntityType(kKnownTravelerNumber)),
            EntityType::KNOWN_TRAVELER_NUMBER);
}

TEST(AutofillAiPersonalContextConverters, MaskSpiiEntityFields) {
  // Passport
  {
    personal_context::proto::Entity entity;
    entity.mutable_passport()->set_name("Jane Doe");
    entity.mutable_passport()->set_number("P12345");
    MaskSpiiEntityFields(entity);
    EXPECT_EQ(entity.passport().name(), "Jane Doe");
    EXPECT_EQ(entity.passport().number(), "45");
  }
  // Drivers License
  {
    personal_context::proto::Entity entity;
    entity.mutable_drivers_license()->set_name("John Smith");
    entity.mutable_drivers_license()->set_number("DL9876");
    MaskSpiiEntityFields(entity);
    EXPECT_EQ(entity.drivers_license().name(), "John Smith");
    EXPECT_EQ(entity.drivers_license().number(), "76");
  }
  // National ID
  {
    personal_context::proto::Entity entity;
    entity.mutable_national_id()->set_name("Alex Doe");
    entity.mutable_national_id()->set_number("4658233983");
    MaskSpiiEntityFields(entity);
    EXPECT_EQ(entity.national_id().name(), "Alex Doe");
    EXPECT_EQ(entity.national_id().number(), "983");
  }
  // Known Traveler Number
  {
    personal_context::proto::Entity entity;
    entity.mutable_known_traveler_number()->set_name("Alice");
    entity.mutable_known_traveler_number()->set_number("T12345678");
    MaskSpiiEntityFields(entity);
    EXPECT_EQ(entity.known_traveler_number().name(), "Alice");
    EXPECT_EQ(entity.known_traveler_number().number(), "678");
  }
  // Non-SPII entity (Order) should not be modified
  {
    personal_context::proto::Entity entity;
    entity.mutable_order()->set_order_id("ORD-12345");
    entity.mutable_order()->set_merchant_name("Merchant");
    MaskSpiiEntityFields(entity);
    EXPECT_EQ(entity.order().order_id(), "ORD-12345");
    EXPECT_EQ(entity.order().merchant_name(), "Merchant");
  }
}

}  // namespace

}  // namespace autofill
