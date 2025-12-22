// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_ai/entity_sync_util.h"

#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance_test_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/autofill_ai_chrome_metadata.pb.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/sync/protocol/autofill_valuable_metadata_specifics.pb.h"
#include "components/sync/protocol/autofill_valuable_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

// Returns the string value of the attribute with the given type in `entity`.
std::string GetStringValue(const EntityInstance& entity,
                           AttributeTypeName attribute_type_name) {
  return base::UTF16ToUTF8(entity.attribute(AttributeType(attribute_type_name))
                               ->GetCompleteRawInfo());
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
      CreateSpecificsFromEntityInstance(vehicle);

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
      CreateSpecificsFromEntityInstance(flight_reservation);

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
      CreateSpecificsFromEntityInstance(flight_reservation);

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
                     EntityInstance::AreAttributesReadOnly(false)}));
    EXPECT_TRUE(specifics.is_editable());
  }
  {
    sync_pb::AutofillValuableSpecifics specifics =
        CreateSpecificsFromEntityInstance(
            test::GetFlightReservationEntityInstance(
                {.are_attributes_read_only =
                     EntityInstance::AreAttributesReadOnly(true)}));
    EXPECT_FALSE(specifics.is_editable());
  }
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
      CreateSpecificsFromEntityMetadata(metadata);

  EXPECT_EQ(specifics.valuable_id(), "test-valuable-id");
  EXPECT_EQ(specifics.last_modified_date_unix_epoch_micros(),
            13379000000000000ll);
  EXPECT_EQ(specifics.use_count(), 5u);
  EXPECT_EQ(specifics.last_used_date_unix_epoch_micros(), 13347400000000000ll);
}

// Tests that the `CreateValuableMetadataFromSpecifics` function correctly
// converts the proto to EntityTable::EntityMetadata.
TEST(EntitySyncUtilTest, CreateValuableMetadataFromSpecifics) {
  sync_pb::AutofillValuableMetadataSpecifics specifics;
  specifics.set_valuable_id("test-valuable-id");
  // Corresponds to Jan 1, 2025, 00:00:00 UTC
  specifics.set_last_modified_date_unix_epoch_micros(13379000000000000u);
  specifics.set_use_count(5);
  // Corresponds to Jan 1, 2024, 00:00:00 UTC
  specifics.set_last_used_date_unix_epoch_micros(13347400000000000u);

  EntityInstance::EntityMetadata metadata =
      CreateValuableMetadataFromSpecifics(specifics);

  EXPECT_EQ(metadata.guid.value(), "test-valuable-id");
  EXPECT_EQ(metadata.date_modified,
            base::Time::FromDeltaSinceWindowsEpoch(
                base::Microseconds(13379000000000000u)));
  EXPECT_EQ(metadata.use_count, 5u);
  EXPECT_EQ(metadata.use_date, base::Time::FromDeltaSinceWindowsEpoch(
                                   base::Microseconds(13347400000000000u)));
}

}  // namespace
}  // namespace autofill
