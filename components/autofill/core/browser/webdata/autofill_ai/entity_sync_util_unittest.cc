// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_ai/entity_sync_util.h"

#include "base/strings/utf_string_conversions.h"
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

}  // namespace

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
}

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
}

}  // namespace autofill
