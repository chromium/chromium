// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_ai/entity_sync_util.h"

#include "base/i18n/time_formatting.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance_test_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/autofill_ai_chrome_metadata.pb.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/sync/protocol/autofill_valuable_metadata_specifics.pb.h"
#include "components/sync/protocol/autofill_valuable_specifics.pb.h"
#include "components/sync/test/unknown_field_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using syncer::test::AddUnknownFieldToProto;
using syncer::test::HasUnknownField;

// Returns the string value of the attribute with the given type in `entity`.
std::string GetStringValue(const EntityInstance& entity,
                           AttributeTypeName attribute_type_name) {
  return base::UTF16ToUTF8(entity.attribute(AttributeType(attribute_type_name))
                               ->GetCompleteRawInfo());
}

std::string GetDateValue(const EntityInstance& entity,
                         AttributeTypeName attribute_type_name,
                         const std::u16string& format) {
  AttributeType attribute_type(attribute_type_name);
  return base::UTF16ToUTF8(
      entity.attribute(attribute_type)
          ->GetInfo(attribute_type.field_type(), "en_US",
                    AutofillFormatString(format, FormatString_Type_DATE)));
}

// Returns a `sync_pb::AutofillValuableSpecifics` message with
// the flight reservation entity type.
sync_pb::AutofillValuableSpecifics TestFlightReservationSpecifics(
    test::FlightReservationOptions options = {}) {
  sync_pb::AutofillValuableSpecifics specifics =
      sync_pb::AutofillValuableSpecifics();
  specifics.set_id(options.guid);
  specifics.mutable_flight_reservation()->set_flight_number(
      base::UTF16ToUTF8(options.flight_number));
  specifics.mutable_flight_reservation()->set_flight_ticket_number(
      base::UTF16ToUTF8(options.ticket_number));
  specifics.mutable_flight_reservation()->set_flight_confirmation_code(
      base::UTF16ToUTF8(options.confirmation_code));
  specifics.mutable_flight_reservation()->set_passenger_name(
      base::UTF16ToUTF8(options.name));
  specifics.mutable_flight_reservation()->set_departure_airport(
      base::UTF16ToUTF8(options.departure_airport));
  specifics.mutable_flight_reservation()->set_arrival_airport(
      base::UTF16ToUTF8(options.arrival_airport));
  if (options.departure_time.has_value()) {
    specifics.mutable_flight_reservation()
        ->set_departure_date_unix_epoch_micros(
            options.departure_time->InMillisecondsSinceUnixEpoch() * 1000);
  }
  specifics.mutable_flight_reservation()
      ->set_departure_airport_utc_offset_seconds(
          options.departure_time_zone_offset.InSeconds());

  ChromeValuablesMetadata metadata;
  ChromeValuablesMetadataEntry& entry = *metadata.add_metadata_entries();
  entry.set_attribute_type("Passenger name");
  entry.set_field_type(static_cast<int>(FieldType::NAME_FULL));
  entry.set_value(base::UTF16ToUTF8(options.name));
  entry.set_verification_status(
      static_cast<int>(VerificationStatus::kServerParsed));

  specifics.mutable_serialized_chrome_valuables_metadata()->set_type_url(
      base::StrCat({"type.googleapis.com/", metadata.GetTypeName()}));
  specifics.mutable_serialized_chrome_valuables_metadata()->set_value(
      metadata.SerializeAsString());

  return specifics;
}

// Returns a `sync_pb::AutofillValuableSpecifics` message with
// the vehicle entity type.
sync_pb::AutofillValuableSpecifics TestVehicleSpecifics() {
  sync_pb::AutofillValuableSpecifics specifics =
      sync_pb::AutofillValuableSpecifics();
  specifics.set_id("00000000-0000-4000-8000-200000000000");
  specifics.mutable_vehicle_registration()->set_vehicle_make("Make");
  specifics.mutable_vehicle_registration()->set_vehicle_model("Model");
  specifics.mutable_vehicle_registration()->set_vehicle_year("2025");
  specifics.mutable_vehicle_registration()->set_vehicle_identification_number(
      "12345");
  specifics.mutable_vehicle_registration()->set_vehicle_license_plate("Plate");
  specifics.mutable_vehicle_registration()->set_license_plate_region("Region");
  specifics.mutable_vehicle_registration()->set_license_plate_country("US");
  specifics.mutable_vehicle_registration()->set_owner_name("Owner Name");

  return specifics;
}

// `date_str` is expected to have the form "dd/mm/yyyy".
sync_pb::NaiveDate StringToProtoDate(std::u16string_view date_str) {
  int day = 0, month = 0, year = 0;
  EXPECT_TRUE(base::StringToInt(date_str.substr(0, 2), &day));
  EXPECT_TRUE(base::StringToInt(date_str.substr(3, 2), &month));
  EXPECT_TRUE(base::StringToInt(date_str.substr(6, 4), &year));
  sync_pb::NaiveDate proto;
  proto.set_day(day);
  proto.set_month(month);
  proto.set_year(year);
  return proto;
}

std::u16string ProtoDateToString(const sync_pb::NaiveDate& proto) {
  return base::UTF8ToUTF16(base::StringPrintf("%02d/%02d/%04d", proto.day(),
                                              proto.month(), proto.year()));
}

// Returns a `sync_pb::AutofillValuableSpecifics` message with the passport
// entity type.
sync_pb::AutofillValuableSpecifics TestPassportSpecifics(
    test::PassportEntityOptions options = {}) {
  sync_pb::AutofillValuableSpecifics specifics;
  specifics.set_id(options.guid);
  sync_pb::Passport* passport = specifics.mutable_passport();
  passport->set_masked_number(base::UTF16ToUTF8(options.number));
  passport->set_owner_name(base::UTF16ToUTF8(options.name));
  passport->set_country_code(base::UTF16ToUTF8(options.country));
  *passport->mutable_issue_date() = StringToProtoDate(options.issue_date);
  *passport->mutable_expiration_date() = StringToProtoDate(options.expiry_date);

  return specifics;
}

// Returns a `sync_pb::AutofillValuableSpecifics` message with the driver's
// license entity type.
sync_pb::AutofillValuableSpecifics TestDriversLicenseSpecifics(
    test::DriversLicenseOptions options = {}) {
  sync_pb::AutofillValuableSpecifics specifics;
  specifics.set_id(std::string(options.guid));
  sync_pb::DriverLicense* license = specifics.mutable_driver_license();
  license->set_masked_number(base::UTF16ToUTF8(options.number));
  license->set_owner_name(base::UTF16ToUTF8(options.name));
  license->set_region(base::UTF16ToUTF8(options.region));
  *license->mutable_issue_date() = StringToProtoDate(options.issue_date);
  *license->mutable_expiration_date() =
      StringToProtoDate(options.expiration_date);

  return specifics;
}

// Returns a `sync_pb::AutofillValuableSpecifics` message with the national ID
// card entity type.
sync_pb::AutofillValuableSpecifics TestNationalIdCardSpecifics(
    test::NationalIdCardOptions options = {}) {
  sync_pb::AutofillValuableSpecifics specifics;
  specifics.set_id(std::string(options.guid));
  sync_pb::NationalIdCard* card = specifics.mutable_national_id_card();
  card->set_masked_number(base::UTF16ToUTF8(options.number));
  card->set_owner_name(base::UTF16ToUTF8(options.name));
  card->set_country_code(base::UTF16ToUTF8(options.country));
  *card->mutable_issue_date() = StringToProtoDate(options.issue_date);
  *card->mutable_expiration_date() = StringToProtoDate(options.expiry_date);

  return specifics;
}

// Returns a `sync_pb::AutofillValuableSpecifics` message with the redress
// number entity type.
sync_pb::AutofillValuableSpecifics TestRedressNumberSpecifics(
    test::RedressNumberOptions options = {}) {
  sync_pb::AutofillValuableSpecifics specifics;
  specifics.set_id(std::string(options.guid));
  sync_pb::RedressNumber* redress = specifics.mutable_redress_number();
  redress->set_masked_number(base::UTF16ToUTF8(options.number));
  redress->set_owner_name(base::UTF16ToUTF8(options.name));

  return specifics;
}

// Returns a `sync_pb::AutofillValuableSpecifics` message with the known
// traveler number entity type.
sync_pb::AutofillValuableSpecifics TestKnownTravelerNumberSpecifics(
    test::KnownTravelerNumberOptions options = {}) {
  sync_pb::AutofillValuableSpecifics specifics;
  specifics.set_id(std::string(options.guid));
  sync_pb::KnownTravelerNumber* ktn = specifics.mutable_known_traveler_number();
  ktn->set_masked_number(base::UTF16ToUTF8(options.number));
  ktn->set_owner_name(base::UTF16ToUTF8(options.name));
  *ktn->mutable_expiration_date() = StringToProtoDate(options.expiration_date);

  return specifics;
}

TEST(EntitySyncUtilTest, CreateEntityDataFromEntityInstance) {
  EntityInstance vehicle_entity = test::GetVehicleEntityInstance();
  std::unique_ptr<syncer::EntityData> entity_data =
      CreateEntityDataFromEntityInstance(vehicle_entity,
                                         /*base_specifics=*/{});

  sync_pb::AutofillValuableSpecifics specifics =
      entity_data->specifics.autofill_valuable();
  ASSERT_TRUE(entity_data->specifics.has_autofill_valuable());

  const sync_pb::VehicleRegistration& vehicle_specifics =
      specifics.vehicle_registration();

  EXPECT_EQ(vehicle_entity.guid().value(), specifics.id());
  EXPECT_EQ(
      vehicle_entity.attribute(AttributeType(AttributeTypeName::kVehicleMake))
          ->GetCompleteRawInfo(),
      base::UTF8ToUTF16(vehicle_specifics.vehicle_make()));
  EXPECT_EQ(
      vehicle_entity.attribute(AttributeType(AttributeTypeName::kVehicleModel))
          ->GetCompleteRawInfo(),
      base::UTF8ToUTF16(vehicle_specifics.vehicle_model()));
  EXPECT_EQ(
      vehicle_entity.attribute(AttributeType(AttributeTypeName::kVehicleYear))
          ->GetCompleteRawInfo(),
      base::UTF8ToUTF16(vehicle_specifics.vehicle_year()));
  EXPECT_EQ(
      vehicle_entity.attribute(AttributeType(AttributeTypeName::kVehicleOwner))
          ->GetCompleteRawInfo(),
      base::UTF8ToUTF16(vehicle_specifics.owner_name()));
  EXPECT_EQ(
      vehicle_entity
          .attribute(AttributeType(AttributeTypeName::kVehiclePlateNumber))
          ->GetCompleteRawInfo(),
      base::UTF8ToUTF16(vehicle_specifics.vehicle_license_plate()));
  EXPECT_EQ(
      vehicle_entity.attribute(AttributeType(AttributeTypeName::kVehicleVin))
          ->GetCompleteRawInfo(),
      base::UTF8ToUTF16(vehicle_specifics.vehicle_identification_number()));
  EXPECT_EQ(vehicle_entity
                .attribute(AttributeType(AttributeTypeName::kVehiclePlateState))
                ->GetCompleteRawInfo(),
            base::UTF8ToUTF16(vehicle_specifics.license_plate_region()));
}

// Tests that the `CreateEntityInstanceFromSpecifics` function correctly
// deserializes the vehicle entity from its proto representation.
TEST(EntitySyncUtilTest, CreateEntityInstanceFromSpecifics_Vehicle) {
  sync_pb::AutofillValuableSpecifics specifics = TestVehicleSpecifics();
  std::optional<EntityInstance> vehicle =
      CreateEntityInstanceFromSpecifics(specifics);
  ASSERT_TRUE(vehicle.has_value());
  EXPECT_EQ(vehicle->guid().value(), specifics.id());
  EXPECT_EQ(GetStringValue(*vehicle, AttributeTypeName::kVehicleMake),
            specifics.vehicle_registration().vehicle_make());
  EXPECT_EQ(GetStringValue(*vehicle, AttributeTypeName::kVehicleModel),
            specifics.vehicle_registration().vehicle_model());
  EXPECT_EQ(GetStringValue(*vehicle, AttributeTypeName::kVehicleYear),
            specifics.vehicle_registration().vehicle_year());
  EXPECT_EQ(GetStringValue(*vehicle, AttributeTypeName::kVehicleOwner),
            specifics.vehicle_registration().owner_name());
  EXPECT_EQ(GetStringValue(*vehicle, AttributeTypeName::kVehiclePlateNumber),
            specifics.vehicle_registration().vehicle_license_plate());
  EXPECT_EQ(GetStringValue(*vehicle, AttributeTypeName::kVehiclePlateState),
            specifics.vehicle_registration().license_plate_region());
  EXPECT_EQ(GetStringValue(*vehicle, AttributeTypeName::kVehicleVin),
            specifics.vehicle_registration().vehicle_identification_number());
}

// Tests that the `CreateSpecificsFromEntityInstance` function correctly
// serializes the vehicle entity into its proto representation.
TEST(EntitySyncUtilTest, CreateSpecificsFromEntityInstance_Vehicle) {
  EntityInstance vehicle = test::GetVehicleEntityInstance();

  sync_pb::AutofillValuableSpecifics specifics =
      CreateSpecificsFromEntityInstance(vehicle,
                                        /*base_specifics=*/{});

  EXPECT_EQ(vehicle.guid().value(), specifics.id());
  EXPECT_EQ(GetStringValue(vehicle, AttributeTypeName::kVehicleMake),
            specifics.vehicle_registration().vehicle_make());
  EXPECT_EQ(GetStringValue(vehicle, AttributeTypeName::kVehicleModel),
            specifics.vehicle_registration().vehicle_model());
  EXPECT_EQ(GetStringValue(vehicle, AttributeTypeName::kVehicleYear),
            specifics.vehicle_registration().vehicle_year());
  EXPECT_EQ(GetStringValue(vehicle, AttributeTypeName::kVehicleOwner),
            specifics.vehicle_registration().owner_name());
  EXPECT_EQ(GetStringValue(vehicle, AttributeTypeName::kVehiclePlateNumber),
            specifics.vehicle_registration().vehicle_license_plate());
  EXPECT_EQ(GetStringValue(vehicle, AttributeTypeName::kVehiclePlateState),
            specifics.vehicle_registration().license_plate_region());
  EXPECT_EQ(GetStringValue(vehicle, AttributeTypeName::kVehicleVin),
            specifics.vehicle_registration().vehicle_identification_number());
}

// Tests that in case of a conflicting name between the vehicle owner field and
// valuables_metadata, the vehicle owner field is prioritized.
TEST(EntitySyncUtilTest,
     CreateEntityInstanceFromSpecifics_Vehicle_ConflictingOwnerMetadata) {
  sync_pb::AutofillValuableSpecifics specifics = TestVehicleSpecifics();
  specifics.mutable_vehicle_registration()->set_owner_name("Original Name");

  // Set conflicting metadata.
  {
    ChromeValuablesMetadata metadata;
    ChromeValuablesMetadataEntry& entry = *metadata.add_metadata_entries();
    entry.set_attribute_type(
        AttributeType(AttributeTypeName::kVehicleOwner).name_as_string());
    entry.set_field_type(static_cast<int>(FieldType::NAME_FULL));
    entry.set_value("Conflicting Name");
    entry.set_verification_status(
        static_cast<int>(VerificationStatus::kServerParsed));
    specifics.mutable_serialized_chrome_valuables_metadata()->set_type_url(
        base::StrCat({"type.googleapis.com/", metadata.GetTypeName()}));
    specifics.mutable_serialized_chrome_valuables_metadata()->set_value(
        metadata.SerializeAsString());
  };

  std::optional<EntityInstance> vehicle =
      CreateEntityInstanceFromSpecifics(specifics);

  ASSERT_TRUE(vehicle.has_value());
  EXPECT_EQ(vehicle->guid().value(), specifics.id());
  // Validate that the original (not the metadata conflicting) name is used.
  EXPECT_EQ(GetStringValue(*vehicle, AttributeTypeName::kVehicleOwner),
            specifics.vehicle_registration().owner_name());
  // Validate other fields.
  EXPECT_EQ(GetStringValue(*vehicle, AttributeTypeName::kVehicleMake),
            specifics.vehicle_registration().vehicle_make());
  EXPECT_EQ(GetStringValue(*vehicle, AttributeTypeName::kVehicleModel),
            specifics.vehicle_registration().vehicle_model());
  EXPECT_EQ(GetStringValue(*vehicle, AttributeTypeName::kVehicleYear),
            specifics.vehicle_registration().vehicle_year());

  EXPECT_EQ(GetStringValue(*vehicle, AttributeTypeName::kVehiclePlateNumber),
            specifics.vehicle_registration().vehicle_license_plate());
  EXPECT_EQ(GetStringValue(*vehicle, AttributeTypeName::kVehiclePlateState),
            specifics.vehicle_registration().license_plate_region());
  EXPECT_EQ(GetStringValue(*vehicle, AttributeTypeName::kVehicleVin),
            specifics.vehicle_registration().vehicle_identification_number());
}

// Tests that the `CreateSpecificsFromEntityInstance` function correctly
// merges the `base_specifics` into the result for the vehicle entity.
TEST(EntitySyncUtilTest,
     CreateSpecificsFromEntityInstance_Vehicle_MergesBaseSpecifics) {
  sync_pb::AutofillValuableSpecifics base_specifics;
  AddUnknownFieldToProto(base_specifics, "unknown_field");

  EntityInstance vehicle = test::GetVehicleEntityInstance();
  sync_pb::AutofillValuableSpecifics specifics =
      CreateSpecificsFromEntityInstance(vehicle, base_specifics);

  EXPECT_EQ(specifics.vehicle_registration().vehicle_make(),
            GetStringValue(vehicle, AttributeTypeName::kVehicleMake));
  EXPECT_THAT(specifics, HasUnknownField("unknown_field"));
}

// Tests that `CreateSpecificsFromEntityInstance` clears fields in
// `base_specifics` if the corresponding attribute is missing in the vehicle
// entity.
TEST(EntitySyncUtilTest,
     CreateSpecificsFromEntityInstance_Vehicle_ClearsMissingFields) {
  sync_pb::AutofillValuableSpecifics base_specifics = TestVehicleSpecifics();

  // Create a vehicle entity with only the Make attribute.
  base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
      attributes;
  AttributeInstance make_attr((AttributeType(AttributeTypeName::kVehicleMake)));
  make_attr.SetRawInfo(
      AttributeType(AttributeTypeName::kVehicleMake).field_type(), u"New Make",
      VerificationStatus::kNoStatus);
  attributes.insert(make_attr);

  EntityInstance vehicle(
      EntityType(EntityTypeName::kVehicle), std::move(attributes),
      EntityInstance::EntityId("00000000-0000-4000-8000-200000000000"),
      /*nickname=*/"", /*date_modified=*/{}, /*use_count=*/0, /*use_date=*/{},
      EntityInstance::RecordType::kServerWallet,
      EntityInstance::AreAttributesReadOnly(false),
      /*frecency_override=*/"");

  sync_pb::AutofillValuableSpecifics specifics =
      CreateSpecificsFromEntityInstance(vehicle, base_specifics);

  EXPECT_EQ(specifics.vehicle_registration().vehicle_make(), "New Make");
  EXPECT_FALSE(specifics.vehicle_registration().has_vehicle_model());
  EXPECT_FALSE(specifics.vehicle_registration().has_vehicle_year());
  EXPECT_FALSE(
      specifics.vehicle_registration().has_vehicle_identification_number());
  EXPECT_FALSE(specifics.vehicle_registration().has_vehicle_license_plate());
  EXPECT_FALSE(specifics.vehicle_registration().has_license_plate_region());
  EXPECT_FALSE(specifics.vehicle_registration().has_owner_name());
}

// Tests that the `CreateEntityInstanceFromSpecifics` function correctly
// deserializes the flight reservation entity from its proto representation.
TEST(EntitySyncUtilTest, CreateEntityInstanceFromSpecifics_FlightReservation) {
  base::Time departure_time;
  ASSERT_TRUE(base::Time::FromUTCString("2025-01-01", &departure_time));
  sync_pb::AutofillValuableSpecifics specifics =
      TestFlightReservationSpecifics({.departure_time = departure_time});
  std::optional<EntityInstance> flight_reservation =
      CreateEntityInstanceFromSpecifics(specifics);

  ASSERT_TRUE(flight_reservation.has_value());
  EXPECT_EQ(flight_reservation->guid().value(), specifics.id());
  EXPECT_EQ(GetStringValue(*flight_reservation,
                           AttributeTypeName::kFlightReservationFlightNumber),
            specifics.flight_reservation().flight_number());
  EXPECT_EQ(GetStringValue(*flight_reservation,
                           AttributeTypeName::kFlightReservationTicketNumber),
            specifics.flight_reservation().flight_ticket_number());
  EXPECT_EQ(
      GetStringValue(*flight_reservation,
                     AttributeTypeName::kFlightReservationConfirmationCode),
      specifics.flight_reservation().flight_confirmation_code());
  EXPECT_EQ(
      GetStringValue(*flight_reservation,
                     AttributeTypeName::kFlightReservationDepartureAirport),
      specifics.flight_reservation().departure_airport());
  EXPECT_EQ(GetStringValue(*flight_reservation,
                           AttributeTypeName::kFlightReservationArrivalAirport),
            specifics.flight_reservation().arrival_airport());
  EXPECT_EQ(GetStringValue(*flight_reservation,
                           AttributeTypeName::kFlightReservationDepartureDate),
            "2025-01-01");
  const AttributeInstance& name = *flight_reservation->attribute(
      AttributeType(AttributeTypeName::kFlightReservationPassengerName));
  EXPECT_EQ(name.GetRawInfo(FieldType::NAME_FULL),
            base::UTF8ToUTF16(specifics.flight_reservation().passenger_name()));
  EXPECT_EQ(name.GetVerificationStatus(FieldType::NAME_FULL),
            VerificationStatus::kServerParsed);
  EXPECT_EQ(test_api(*flight_reservation).frecency_override(),
            base::TimeFormatAsIso8601(departure_time));
}

// Tests that the `CreateEntityInstanceFromSpecifics` function correctly
// deserializes the flight reservation entity when departure time is empty.
TEST(EntitySyncUtilTest,
     CreateEntityInstanceFromSpecifics_FlightReservation_EmptyDepartureTime) {
  sync_pb::AutofillValuableSpecifics specifics =
      TestFlightReservationSpecifics();

  std::optional<EntityInstance> flight_reservation =
      CreateEntityInstanceFromSpecifics(specifics);

  ASSERT_TRUE(flight_reservation.has_value());
  EXPECT_EQ(test_api(*flight_reservation).frecency_override(), "");
}

// Tests that the `CreateSpecificsFromEntityInstance` function correctly
// serializes the flight reservation entity into its proto representation.
TEST(EntitySyncUtilTest, CreateSpecificsFromEntityInstance_FlightReservation) {
  EntityInstance flight_reservation =
      test::GetFlightReservationEntityInstance();

  sync_pb::AutofillValuableSpecifics specifics =
      CreateSpecificsFromEntityInstance(flight_reservation,
                                        /*base_specifics=*/{});

  EXPECT_EQ(flight_reservation.guid().value(), specifics.id());
  EXPECT_EQ(GetStringValue(flight_reservation,
                           AttributeTypeName::kFlightReservationFlightNumber),
            specifics.flight_reservation().flight_number());
  EXPECT_EQ(GetStringValue(flight_reservation,
                           AttributeTypeName::kFlightReservationTicketNumber),
            specifics.flight_reservation().flight_ticket_number());
  EXPECT_EQ(
      GetStringValue(flight_reservation,
                     AttributeTypeName::kFlightReservationConfirmationCode),
      specifics.flight_reservation().flight_confirmation_code());
  EXPECT_EQ(GetStringValue(flight_reservation,
                           AttributeTypeName::kFlightReservationPassengerName),
            specifics.flight_reservation().passenger_name());
  EXPECT_EQ(
      GetStringValue(flight_reservation,
                     AttributeTypeName::kFlightReservationDepartureAirport),
      specifics.flight_reservation().departure_airport());
  EXPECT_EQ(GetStringValue(flight_reservation,
                           AttributeTypeName::kFlightReservationArrivalAirport),
            specifics.flight_reservation().arrival_airport());
  EXPECT_FALSE(
      specifics.flight_reservation().has_departure_date_unix_epoch_micros());
}

// Tests that the `CreateSpecificsFromEntityInstance` function ignores the
// departure time when serializing the flight reservation entity.
TEST(EntitySyncUtilTest,
     CreateSpecificsFromEntityInstance_FlightReservation_IgnoresDepartureTime) {
  EntityInstance flight_reservation = test::GetFlightReservationEntityInstance({
      .departure_time = base::Time::UnixEpoch() + base::Seconds(1000),
  });

  sync_pb::AutofillValuableSpecifics specifics =
      CreateSpecificsFromEntityInstance(flight_reservation,
                                        /*base_specifics=*/{});

  EXPECT_FALSE(
      specifics.flight_reservation().has_departure_date_unix_epoch_micros());
}

// Tests that the `CreateEntityInstanceFromSpecifics` function correctly
// aligns the departure date with the departure airport's time zone offset when
// the offset is positive.
TEST(
    EntitySyncUtilTest,
    CreateSpecificsFromEntityInstance_FlightReservation_AlignsDateWithPositiveTimeZoneOffset) {
  base::Time departure_time;
  ASSERT_TRUE(
      base::Time::FromUTCString("2025-01-01T23:00:00", &departure_time));
  sync_pb::AutofillValuableSpecifics specifics =
      TestFlightReservationSpecifics({
          .departure_time = departure_time,
          .departure_time_zone_offset = base::Hours(1),
      });

  EXPECT_EQ(GetStringValue(*CreateEntityInstanceFromSpecifics(specifics),
                           AttributeTypeName::kFlightReservationDepartureDate),
            "2025-01-02");
}

// Tests that the `CreateEntityInstanceFromSpecifics` function correctly
// aligns the departure date with the departure airport's time zone offset when
// the offset is negative.
TEST(
    EntitySyncUtilTest,
    CreateSpecificsFromEntityInstance_FlightReservation_AlignsDateWithNegativeTimeZoneOffset) {
  base::Time departure_time;
  ASSERT_TRUE(
      base::Time::FromUTCString("2025-01-01T00:00:00", &departure_time));
  sync_pb::AutofillValuableSpecifics specifics =
      TestFlightReservationSpecifics({
          .departure_time = departure_time,
          .departure_time_zone_offset = base::Hours(-1),
      });

  EXPECT_EQ(GetStringValue(*CreateEntityInstanceFromSpecifics(specifics),
                           AttributeTypeName::kFlightReservationDepartureDate),
            "2024-12-31");
}

// Tests that the `CreateEntityInstanceFromSpecifics` function
// does not align the datetime stored in the frecency override with the
// departure airport's time zone offset.
TEST(
    EntitySyncUtilTest,
    CreateSpecificsFromEntityInstance_FlightReservation_FrecencyOverrideAlignedToUTC) {
  base::Time departure_time1;
  ASSERT_TRUE(
      base::Time::FromUTCString("2025-01-01T10:00:00", &departure_time1));
  base::Time departure_time2;
  ASSERT_TRUE(
      base::Time::FromUTCString("2025-01-01T11:00:00", &departure_time2));
  sync_pb::AutofillValuableSpecifics specifics1 =
      TestFlightReservationSpecifics(
          {.departure_time = departure_time1,
           .departure_time_zone_offset = base::Hours(2)});
  sync_pb::AutofillValuableSpecifics specifics2 =
      TestFlightReservationSpecifics(
          {.departure_time = departure_time2,
           .departure_time_zone_offset = base::Hours(-1)});
  EntityInstance entity1 = *CreateEntityInstanceFromSpecifics(specifics1);
  EntityInstance entity2 = *CreateEntityInstanceFromSpecifics(specifics2);

  // If time zone offsets were subtracted from the timestamps,
  // `entity2` would have a frecency override that is smaller than `entity1`.
  // We want to test this is **not** the case.
  EXPECT_LT(test_api(entity1).frecency_override(),
            test_api(entity2).frecency_override());
}

// Tests that the `CreateSpecificsFromEntityInstance` function correctly
// merges the `base_specifics` into the result for the flight reservation
// entity.
TEST(EntitySyncUtilTest,
     CreateSpecificsFromEntityInstance_FlightReservation_MergesBaseSpecifics) {
  sync_pb::AutofillValuableSpecifics base_specifics;
  AddUnknownFieldToProto(base_specifics, "unknown_field");

  EntityInstance flight_reservation =
      test::GetFlightReservationEntityInstance();
  sync_pb::AutofillValuableSpecifics specifics =
      CreateSpecificsFromEntityInstance(flight_reservation, base_specifics);

  EXPECT_EQ(specifics.flight_reservation().passenger_name(),
            GetStringValue(flight_reservation,
                           AttributeTypeName::kFlightReservationPassengerName));
  EXPECT_THAT(specifics, HasUnknownField("unknown_field"));
}

// Tests that the `CreateEntityInstanceFromSpecifics` function correctly sets
// the `are_attributes_read_only` property.
TEST(EntitySyncUtilTest, CreateEntityInstanceFromSpecifics_IsEditable) {
  {
    sync_pb::AutofillValuableSpecifics specifics =
        TestFlightReservationSpecifics();

    specifics.set_is_editable(true);
    std::optional<EntityInstance> flight_reservation =
        CreateEntityInstanceFromSpecifics(specifics);
    ASSERT_TRUE(flight_reservation.has_value());
    EXPECT_FALSE(flight_reservation->are_attributes_read_only());
  }
  {
    sync_pb::AutofillValuableSpecifics specifics =
        TestFlightReservationSpecifics();
    specifics.set_is_editable(false);
    std::optional<EntityInstance> flight_reservation =
        CreateEntityInstanceFromSpecifics(specifics);
    ASSERT_TRUE(flight_reservation.has_value());
    EXPECT_TRUE(flight_reservation->are_attributes_read_only());
  }
}

// Tests that the `CreateSpecificsFromEntityInstance` function correctly sets
// the `is_editable` property.
TEST(EntitySyncUtilTest, CreateSpecificsFromEntityInstance_IsEditable) {
  {
    sync_pb::AutofillValuableSpecifics specifics =
        CreateSpecificsFromEntityInstance(
            test::GetFlightReservationEntityInstance(
                {.are_attributes_read_only =
                     EntityInstance::AreAttributesReadOnly(false)}),
            /*base_specifics=*/{});
    EXPECT_TRUE(specifics.is_editable());
  }
  {
    sync_pb::AutofillValuableSpecifics specifics =
        CreateSpecificsFromEntityInstance(
            test::GetFlightReservationEntityInstance(
                {.are_attributes_read_only =
                     EntityInstance::AreAttributesReadOnly(true)}),
            /*base_specifics=*/{});
    EXPECT_FALSE(specifics.is_editable());
  }
}

TEST(EntitySyncUtilTest, CreateEntityDataFromEntityMetadata) {
  EntityInstance::EntityMetadata metadata;
  metadata.guid = EntityInstance::EntityId("test-valuable-id");
  metadata.date_modified = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(13379000000000000u));
  metadata.use_count = 5;
  metadata.use_date = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(13347400000000000u));

  std::unique_ptr<syncer::EntityData> entity_data =
      CreateEntityDataFromEntityMetadata(
          metadata,
          sync_pb::AutofillValuableMetadataSpecifics::VEHICLE_REGISTRATION,
          /*base_specifics=*/{});

  EXPECT_EQ(entity_data->name, metadata.guid.value());
  EXPECT_EQ(entity_data->specifics.autofill_valuable_metadata().valuable_id(),
            metadata.guid.value());
  EXPECT_EQ(entity_data->specifics.autofill_valuable_metadata()
                .last_modified_date_unix_epoch_micros(),
            metadata.date_modified.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_EQ(entity_data->specifics.autofill_valuable_metadata()
                .last_used_date_unix_epoch_micros(),
            metadata.use_date.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_EQ(entity_data->specifics.autofill_valuable_metadata().pass_type(),
            sync_pb::AutofillValuableMetadataSpecifics::VEHICLE_REGISTRATION);
}

// Tests that the `CreateSpecificsFromEntityMetadata` function correctly
// converts EntityTable::EntityMetadata to the proto.
TEST(EntitySyncUtilTest, CreateSpecificsFromEntityMetadata) {
  EntityInstance::EntityMetadata metadata;
  metadata.guid = EntityInstance::EntityId("test-valuable-id");
  // Corresponds to Jan 1, 2025, 00:00:00 UTC
  metadata.date_modified = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(13379000000000000u));
  metadata.use_count = 5;
  // Corresponds to Jan 1, 2024, 00:00:00 UTC
  metadata.use_date = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(13347400000000000u));

  sync_pb::AutofillValuableMetadataSpecifics specifics =
      CreateSpecificsFromEntityMetadata(
          metadata,
          sync_pb::AutofillValuableMetadataSpecifics::VEHICLE_REGISTRATION,
          /*base_specifics=*/{});

  EXPECT_EQ(specifics.valuable_id(), "test-valuable-id");
  EXPECT_EQ(specifics.last_modified_date_unix_epoch_micros(),
            13379000000000000ll);
  EXPECT_EQ(specifics.use_count(), 5u);
  EXPECT_EQ(specifics.last_used_date_unix_epoch_micros(), 13347400000000000ll);
  EXPECT_EQ(specifics.pass_type(),
            sync_pb::AutofillValuableMetadataSpecifics::VEHICLE_REGISTRATION);
}

// Tests that the `CreateSpecificsFromEntityMetadata` function correctly
// merges the `base_specifics` into the result.
TEST(EntitySyncUtilTest,
     CreateSpecificsFromEntityMetadata_MergesBaseSpecifics) {
  EntityInstance::EntityMetadata metadata;
  metadata.guid = EntityInstance::EntityId("test-valuable-id");
  metadata.date_modified = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(13379000000000000u));
  metadata.use_count = 5;
  metadata.use_date = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(13347400000000000u));

  sync_pb::AutofillValuableMetadataSpecifics base_specifics;
  AddUnknownFieldToProto(base_specifics, "unknown_field");

  sync_pb::AutofillValuableMetadataSpecifics specifics =
      CreateSpecificsFromEntityMetadata(
          metadata,
          sync_pb::AutofillValuableMetadataSpecifics::VEHICLE_REGISTRATION,
          base_specifics);

  EXPECT_EQ(specifics.valuable_id(), "test-valuable-id");
  EXPECT_THAT(specifics, HasUnknownField("unknown_field"));
}

// Tests that the `CreateEntityMetadataFromSpecifics` function correctly
// converts the proto to EntityTable::EntityMetadata.
TEST(EntitySyncUtilTest, CreateEntityMetadataFromSpecifics) {
  sync_pb::AutofillValuableMetadataSpecifics specifics;
  specifics.set_valuable_id("test-valuable-id");
  // Corresponds to Jan 1, 2025, 00:00:00 UTC
  specifics.set_last_modified_date_unix_epoch_micros(13379000000000000u);
  specifics.set_use_count(5);
  // Corresponds to Jan 1, 2024, 00:00:00 UTC
  specifics.set_last_used_date_unix_epoch_micros(13347400000000000u);

  EntityInstance::EntityMetadata metadata =
      CreateEntityMetadataFromSpecifics(specifics);

  EXPECT_EQ(metadata.guid.value(), "test-valuable-id");
  EXPECT_EQ(metadata.date_modified,
            base::Time::FromDeltaSinceWindowsEpoch(
                base::Microseconds(13379000000000000u)));
  EXPECT_EQ(metadata.use_count, 5u);
  EXPECT_EQ(metadata.use_date, base::Time::FromDeltaSinceWindowsEpoch(
                                   base::Microseconds(13347400000000000u)));
}

// Tests that the `EntityTypeNameToPassType` function correctly maps
// EntityTypeName to AutofillValuableMetadataSpecifics::PassType.
TEST(EntitySyncUtilTest, EntityTypeToPassType) {
  using enum EntityTypeName;
  EXPECT_EQ(EntityTypeToPassType(EntityType(kFlightReservation)),
            sync_pb::AutofillValuableMetadataSpecifics::FLIGHT_RESERVATION);
  EXPECT_EQ(EntityTypeToPassType(EntityType(kVehicle)),
            sync_pb::AutofillValuableMetadataSpecifics::VEHICLE_REGISTRATION);
  EXPECT_EQ(EntityTypeToPassType(EntityType(kPassport)),
            sync_pb::AutofillValuableMetadataSpecifics::PASSPORT);
  EXPECT_EQ(EntityTypeToPassType(EntityType(kDriversLicense)),
            sync_pb::AutofillValuableMetadataSpecifics::DRIVER_LICENSE);
  EXPECT_EQ(EntityTypeToPassType(EntityType(kNationalIdCard)),
            sync_pb::AutofillValuableMetadataSpecifics::NATIONAL_ID_CARD);
  EXPECT_EQ(EntityTypeToPassType(EntityType(kRedressNumber)),
            sync_pb::AutofillValuableMetadataSpecifics::REDRESS_NUMBER);
  EXPECT_EQ(EntityTypeToPassType(EntityType(kKnownTravelerNumber)),
            sync_pb::AutofillValuableMetadataSpecifics::KNOWN_TRAVELER_NUMBER);
  EXPECT_FALSE(EntityTypeToPassType(EntityType(kOrder)).has_value());
}

// Tests that `CreateEntityInstanceFromSpecifics` correctly deserializes
// the passport entity from its proto representation.
TEST(EntitySyncUtilTest, CreateEntityInstanceFromSpecifics_Passport) {
  // The specifics require country code.
  test::PassportEntityOptions options{.country = u"DE",
                                      .expiry_date = u"30/08/2019",
                                      .issue_date = u"01/09/2010"};
  sync_pb::AutofillValuableSpecifics specifics = TestPassportSpecifics(options);
  std::optional<EntityInstance> passport =
      CreateEntityInstanceFromSpecifics(specifics);

  ASSERT_TRUE(passport.has_value());
  EXPECT_EQ(passport->guid().value(), specifics.id());
  EXPECT_EQ(GetStringValue(*passport, AttributeTypeName::kPassportNumber),
            specifics.passport().masked_number());
  EXPECT_TRUE(
      passport->attribute(AttributeType(AttributeTypeName::kPassportNumber))
          ->masked());
  EXPECT_EQ(GetStringValue(*passport, AttributeTypeName::kPassportName),
            specifics.passport().owner_name());
  EXPECT_EQ(GetStringValue(*passport, AttributeTypeName::kPassportCountry),
            specifics.passport().country_code());
  EXPECT_EQ(GetDateValue(*passport, AttributeTypeName::kPassportIssueDate,
                         u"DD/MM/YYYY"),
            base::UTF16ToUTF8(options.issue_date));
  EXPECT_EQ(GetDateValue(*passport, AttributeTypeName::kPassportExpirationDate,
                         u"DD/MM/YYYY"),
            base::UTF16ToUTF8(options.expiry_date));
  EXPECT_EQ(passport->record_type(), EntityInstance::RecordType::kServerWallet);
}

// Tests that `CreateSpecificsFromEntityInstance` correctly serializes
// fields.
TEST(EntitySyncUtilTest, CreateSpecificsFromEntityInstance_Passport) {
  // The specifics require country code.
  test::PassportEntityOptions options{.country = u"DE",
                                      .expiry_date = u"30/08/2019",
                                      .issue_date = u"01/09/2010"};
  std::optional<EntityInstance> maybe_passport =
      CreateEntityInstanceFromSpecifics(TestPassportSpecifics(options));
  ASSERT_TRUE(maybe_passport.has_value());
  EntityInstance passport = *maybe_passport;

  sync_pb::AutofillValuableSpecifics specifics =
      CreateSpecificsFromEntityInstance(passport, /*base_specifics=*/{});

  EXPECT_EQ(passport.guid().value(), specifics.id());
  EXPECT_EQ(GetStringValue(passport, AttributeTypeName::kPassportNumber),
            specifics.passport().masked_number());
  EXPECT_EQ(GetStringValue(passport, AttributeTypeName::kPassportName),
            specifics.passport().owner_name());
  EXPECT_EQ(GetStringValue(passport, AttributeTypeName::kPassportCountry),
            specifics.passport().country_code());
  EXPECT_EQ(ProtoDateToString(specifics.passport().issue_date()),
            options.issue_date);
  EXPECT_EQ(ProtoDateToString(specifics.passport().expiration_date()),
            options.expiry_date);
}

// Tests that `CreateEntityInstanceFromSpecifics` correctly deserializes
// the driver's license entity from its proto representation.
TEST(EntitySyncUtilTest, CreateEntityInstanceFromSpecifics_DriverLicense) {
  test::DriversLicenseOptions options;
  sync_pb::AutofillValuableSpecifics specifics =
      TestDriversLicenseSpecifics(options);
  std::optional<EntityInstance> license =
      CreateEntityInstanceFromSpecifics(specifics);

  ASSERT_TRUE(license.has_value());
  EXPECT_EQ(license->guid().value(), options.guid);
  EXPECT_EQ(GetStringValue(*license, AttributeTypeName::kDriversLicenseNumber),
            base::UTF16ToUTF8(options.number));
  EXPECT_TRUE(
      license
          ->attribute(AttributeType(AttributeTypeName::kDriversLicenseNumber))
          ->masked());
  EXPECT_EQ(GetStringValue(*license, AttributeTypeName::kDriversLicenseName),
            base::UTF16ToUTF8(options.name));
  EXPECT_EQ(GetStringValue(*license, AttributeTypeName::kDriversLicenseState),
            base::UTF16ToUTF8(options.region));
  EXPECT_EQ(GetDateValue(*license, AttributeTypeName::kDriversLicenseIssueDate,
                         u"DD/MM/YYYY"),
            base::UTF16ToUTF8(options.issue_date));
  EXPECT_EQ(
      GetDateValue(*license, AttributeTypeName::kDriversLicenseExpirationDate,
                   u"DD/MM/YYYY"),
      base::UTF16ToUTF8(options.expiration_date));
  EXPECT_EQ(license->record_type(), EntityInstance::RecordType::kServerWallet);
}

// Tests that `CreateSpecificsFromEntityInstance` correctly serializes
// fields.
TEST(EntitySyncUtilTest, CreateSpecificsFromEntityInstance_DriverLicense) {
  test::DriversLicenseOptions options;
  std::optional<EntityInstance> maybe_license =
      CreateEntityInstanceFromSpecifics(TestDriversLicenseSpecifics(options));
  ASSERT_TRUE(maybe_license.has_value());
  EntityInstance license = *maybe_license;

  sync_pb::AutofillValuableSpecifics specifics =
      CreateSpecificsFromEntityInstance(license, /*base_specifics=*/{});

  EXPECT_EQ(license.guid().value(), specifics.id());
  EXPECT_EQ(GetStringValue(license, AttributeTypeName::kDriversLicenseNumber),
            specifics.driver_license().masked_number());
  EXPECT_EQ(GetStringValue(license, AttributeTypeName::kDriversLicenseName),
            specifics.driver_license().owner_name());
  EXPECT_EQ(GetStringValue(license, AttributeTypeName::kDriversLicenseState),
            specifics.driver_license().region());
  EXPECT_EQ(ProtoDateToString(specifics.driver_license().issue_date()),
            options.issue_date);
  EXPECT_EQ(ProtoDateToString(specifics.driver_license().expiration_date()),
            options.expiration_date);
}

// Tests that `CreateEntityInstanceFromSpecifics` correctly deserializes
// the national ID card entity from its proto representation.
TEST(EntitySyncUtilTest, CreateEntityInstanceFromSpecifics_NationalIdCard) {
  // The specifics require country code.
  test::NationalIdCardOptions options{.country = u"DE"};
  sync_pb::AutofillValuableSpecifics specifics =
      TestNationalIdCardSpecifics(options);
  std::optional<EntityInstance> card =
      CreateEntityInstanceFromSpecifics(specifics);

  ASSERT_TRUE(card.has_value());
  EXPECT_EQ(card->guid().value(), options.guid);
  EXPECT_EQ(GetStringValue(*card, AttributeTypeName::kNationalIdCardNumber),
            base::UTF16ToUTF8(options.number));
  EXPECT_TRUE(
      card->attribute(AttributeType(AttributeTypeName::kNationalIdCardNumber))
          ->masked());
  EXPECT_EQ(GetStringValue(*card, AttributeTypeName::kNationalIdCardName),
            base::UTF16ToUTF8(options.name));
  EXPECT_EQ(GetStringValue(*card, AttributeTypeName::kNationalIdCardCountry),
            base::UTF16ToUTF8(options.country));
  EXPECT_EQ(GetDateValue(*card, AttributeTypeName::kNationalIdCardIssueDate,
                         u"DD/MM/YYYY"),
            base::UTF16ToUTF8(options.issue_date));
  EXPECT_EQ(
      GetDateValue(*card, AttributeTypeName::kNationalIdCardExpirationDate,
                   u"DD/MM/YYYY"),
      base::UTF16ToUTF8(options.expiry_date));
  EXPECT_EQ(card->record_type(), EntityInstance::RecordType::kServerWallet);
}

// Tests that `CreateSpecificsFromEntityInstance` correctly serializes
// fields.
TEST(EntitySyncUtilTest, CreateSpecificsFromEntityInstance_NationalIdCard) {
  // The specifics require country code.
  test::NationalIdCardOptions options{.country = u"DE"};
  std::optional<EntityInstance> maybe_card =
      CreateEntityInstanceFromSpecifics(TestNationalIdCardSpecifics(options));
  ASSERT_TRUE(maybe_card.has_value());
  EntityInstance card = *maybe_card;

  sync_pb::AutofillValuableSpecifics specifics =
      CreateSpecificsFromEntityInstance(card, /*base_specifics=*/{});

  EXPECT_EQ(card.guid().value(), specifics.id());
  EXPECT_EQ(GetStringValue(card, AttributeTypeName::kNationalIdCardNumber),
            specifics.national_id_card().masked_number());
  EXPECT_EQ(GetStringValue(card, AttributeTypeName::kNationalIdCardName),
            specifics.national_id_card().owner_name());
  EXPECT_EQ(GetStringValue(card, AttributeTypeName::kNationalIdCardCountry),
            specifics.national_id_card().country_code());
  EXPECT_EQ(ProtoDateToString(specifics.national_id_card().issue_date()),
            options.issue_date);
  EXPECT_EQ(ProtoDateToString(specifics.national_id_card().expiration_date()),
            options.expiry_date);
}

// Tests that `CreateEntityInstanceFromSpecifics` correctly deserializes
// the redress number entity from its proto representation.
TEST(EntitySyncUtilTest, CreateEntityInstanceFromSpecifics_RedressNumber) {
  test::RedressNumberOptions options;
  sync_pb::AutofillValuableSpecifics specifics =
      TestRedressNumberSpecifics(options);
  std::optional<EntityInstance> redress =
      CreateEntityInstanceFromSpecifics(specifics);

  ASSERT_TRUE(redress.has_value());
  EXPECT_EQ(redress->guid().value(), options.guid);
  EXPECT_EQ(GetStringValue(*redress, AttributeTypeName::kRedressNumberNumber),
            base::UTF16ToUTF8(options.number));
  EXPECT_TRUE(
      redress->attribute(AttributeType(AttributeTypeName::kRedressNumberNumber))
          ->masked());
  EXPECT_EQ(GetStringValue(*redress, AttributeTypeName::kRedressNumberName),
            base::UTF16ToUTF8(options.name));
  EXPECT_EQ(redress->record_type(), EntityInstance::RecordType::kServerWallet);
}

// Tests that `CreateSpecificsFromEntityInstance` correctly serializes
// fields.
TEST(EntitySyncUtilTest, CreateSpecificsFromEntityInstance_RedressNumber) {
  test::RedressNumberOptions options;
  std::optional<EntityInstance> maybe_redress =
      CreateEntityInstanceFromSpecifics(TestRedressNumberSpecifics(options));
  ASSERT_TRUE(maybe_redress.has_value());
  EntityInstance redress = *maybe_redress;

  sync_pb::AutofillValuableSpecifics specifics =
      CreateSpecificsFromEntityInstance(redress, /*base_specifics=*/{});

  EXPECT_EQ(redress.guid().value(), specifics.id());
  EXPECT_EQ(GetStringValue(redress, AttributeTypeName::kRedressNumberNumber),
            specifics.redress_number().masked_number());
  EXPECT_EQ(GetStringValue(redress, AttributeTypeName::kRedressNumberName),
            specifics.redress_number().owner_name());
}

// Tests that `CreateEntityInstanceFromSpecifics` correctly deserializes
// the known traveler number entity from its proto representation.
TEST(EntitySyncUtilTest,
     CreateEntityInstanceFromSpecifics_KnownTravelerNumber) {
  test::KnownTravelerNumberOptions options;
  sync_pb::AutofillValuableSpecifics specifics =
      TestKnownTravelerNumberSpecifics(options);
  std::optional<EntityInstance> ktn =
      CreateEntityInstanceFromSpecifics(specifics);

  ASSERT_TRUE(ktn.has_value());
  EXPECT_EQ(ktn->guid().value(), options.guid);
  EXPECT_EQ(GetStringValue(*ktn, AttributeTypeName::kKnownTravelerNumberNumber),
            base::UTF16ToUTF8(options.number));
  EXPECT_TRUE(ktn->attribute(AttributeType(
                                 AttributeTypeName::kKnownTravelerNumberNumber))
                  ->masked());
  EXPECT_EQ(GetStringValue(*ktn, AttributeTypeName::kKnownTravelerNumberName),
            base::UTF16ToUTF8(options.name));
  EXPECT_EQ(
      GetDateValue(*ktn, AttributeTypeName::kKnownTravelerNumberExpirationDate,
                   u"DD/MM/YYYY"),
      base::UTF16ToUTF8(options.expiration_date));
  EXPECT_EQ(ktn->record_type(), EntityInstance::RecordType::kServerWallet);
}

// Tests that `CreateSpecificsFromEntityInstance` correctly serializes
// fields.
TEST(EntitySyncUtilTest,
     CreateSpecificsFromEntityInstance_KnownTravelerNumber) {
  test::KnownTravelerNumberOptions options;
  std::optional<EntityInstance> maybe_ktn = CreateEntityInstanceFromSpecifics(
      TestKnownTravelerNumberSpecifics(options));
  ASSERT_TRUE(maybe_ktn.has_value());
  EntityInstance ktn = *maybe_ktn;

  sync_pb::AutofillValuableSpecifics specifics =
      CreateSpecificsFromEntityInstance(ktn, /*base_specifics=*/{});

  EXPECT_EQ(ktn.guid().value(), specifics.id());
  EXPECT_EQ(GetStringValue(ktn, AttributeTypeName::kKnownTravelerNumberNumber),
            specifics.known_traveler_number().masked_number());
  EXPECT_EQ(GetStringValue(ktn, AttributeTypeName::kKnownTravelerNumberName),
            specifics.known_traveler_number().owner_name());
  EXPECT_EQ(
      ProtoDateToString(specifics.known_traveler_number().expiration_date()),
      options.expiration_date);
}

}  // namespace
}  // namespace autofill
