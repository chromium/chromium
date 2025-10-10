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
sync_pb::AutofillValuableSpecifics TestFlightReservationSpecifics() {
  sync_pb::AutofillValuableSpecifics specifics =
      sync_pb::AutofillValuableSpecifics();
  specifics.set_id("00000000-0000-4000-8000-500000000000");
  specifics.mutable_flight_reservation()->set_flight_number("987654321");
  specifics.mutable_flight_reservation()->set_flight_ticket_number("123123456");
  specifics.mutable_flight_reservation()->set_flight_confirmation_code("0123");
  specifics.mutable_flight_reservation()->set_passenger_name("John Doe");
  specifics.mutable_flight_reservation()->set_departure_airport("MUC");
  specifics.mutable_flight_reservation()->set_arrival_airport("BEY");
  specifics.mutable_flight_reservation()->set_departure_date_unix_epoch_micros(
      base::Time::FromSecondsSinceUnixEpoch(60).InMillisecondsSinceUnixEpoch() *
      1000);

  ChromeValuablesMetadata metadata;
  ChromeValuablesMetadataEntry& entry = *metadata.add_metadata_entries();
  entry.set_attribute_type("Passenger name");
  entry.set_field_type(static_cast<int>(FieldType::NAME_FULL));
  entry.set_value("Joe Smith");
  entry.set_verification_status(
      static_cast<int>(VerificationStatus::kServerParsed));

  specifics.mutable_serialized_chrome_valuables_metadata()->set_type_url(
      base::StrCat({"type.googleapis.com/", metadata.GetTypeName()}));
  specifics.mutable_serialized_chrome_valuables_metadata()->set_value(
      metadata.SerializeAsString());

  return specifics;
}

// TODO(crbug.com/40100455): Add tests for Vehicle entity.

// Tests that the `CreateEntityInstanceFromSpecifics` function correctly
// deserializes the flight reservation entity from its proto representation.
TEST(EntitySyncUtilTest, CreateEntityInstanceFromSpecifics_FlightReservation) {
  sync_pb::AutofillValuableSpecifics specifics =
      TestFlightReservationSpecifics();

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
  EXPECT_EQ(GetStringValue(*flight_reservation,
                           AttributeTypeName::kFlightReservationPassengerName),
            "Joe Smith");  // Name from metadata takes precedence over the
                           // `passenger_name` field.
  EXPECT_EQ(
      GetStringValue(*flight_reservation,
                     AttributeTypeName::kFlightReservationDepartureAirport),
      specifics.flight_reservation().departure_airport());
  EXPECT_EQ(GetStringValue(*flight_reservation,
                           AttributeTypeName::kFlightReservationArrivalAirport),
            specifics.flight_reservation().arrival_airport());
  const AttributeInstance& name = *flight_reservation->attribute(
      AttributeType(AttributeTypeName::kFlightReservationPassengerName));
  EXPECT_EQ(name.GetRawInfo(FieldType::NAME_FULL), u"Joe Smith");
  EXPECT_EQ(name.GetVerificationStatus(FieldType::NAME_FULL),
            VerificationStatus::kServerParsed);
  EXPECT_EQ(
      test_api(*flight_reservation).frecency_override(),
      base::TimeFormatAsIso8601(base::Time::FromSecondsSinceUnixEpoch(60)));
}

// Tests that the `CreateEntityInstanceFromSpecifics` function correctly
// deserializes the flight reservation entity when departure time is empty.
TEST(EntitySyncUtilTest,
     CreateEntityInstanceFromSpecifics_FlightReservation_EmptyDepartureTime) {
  sync_pb::AutofillValuableSpecifics specifics =
      TestFlightReservationSpecifics();
  specifics.mutable_flight_reservation()
      ->clear_departure_date_unix_epoch_micros();

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

}  // namespace
}  // namespace autofill
