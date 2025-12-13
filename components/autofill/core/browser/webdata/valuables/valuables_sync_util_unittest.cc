// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuables_sync_util.h"

#include "base/strings/utf_string_conversions.h"
#include "base/types/zip.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/valuables/valuables_sync_test_utils.h"
#include "components/sync/protocol/autofill_valuable_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {
constexpr char kId1[] = "1";
}  // namespace

TEST(LoyaltyCardSyncUtilTest, CreateValuableSpecificsFromLoyaltyCard) {
  LoyaltyCard card = TestLoyaltyCard();
  sync_pb::AutofillValuableSpecifics specifics =
      CreateSpecificsFromLoyaltyCard(card);

  EXPECT_EQ(card.id().value(), specifics.id());
  EXPECT_EQ(card.merchant_name(), specifics.loyalty_card().merchant_name());
  EXPECT_EQ(card.program_name(), specifics.loyalty_card().program_name());
  EXPECT_EQ(card.program_logo(), specifics.loyalty_card().program_logo());
  EXPECT_EQ(card.loyalty_card_number(),
            specifics.loyalty_card().loyalty_card_number());
  ASSERT_EQ(card.merchant_domains().size(),
            (size_t)specifics.loyalty_card().merchant_domains().size());
  for (size_t i = 0; i < card.merchant_domains().size(); i++) {
    EXPECT_EQ(card.merchant_domains()[i],
              specifics.loyalty_card().merchant_domains(i));
  }
}

TEST(LoyaltyCardSyncUtilTest, CreateEntityDataFromLoyaltyCard) {
  LoyaltyCard card = TestLoyaltyCard();
  std::unique_ptr<syncer::EntityData> entity_data =
      CreateEntityDataFromLoyaltyCard(card);

  sync_pb::AutofillValuableSpecifics specifics =
      entity_data->specifics.autofill_valuable();

  EXPECT_TRUE(entity_data->specifics.has_autofill_valuable());
  EXPECT_EQ(card.id().value(), specifics.id());
  EXPECT_EQ(card.merchant_name(), specifics.loyalty_card().merchant_name());
  EXPECT_EQ(card.program_name(), specifics.loyalty_card().program_name());
  EXPECT_EQ(card.program_logo(), specifics.loyalty_card().program_logo());
  EXPECT_EQ(card.loyalty_card_number(),
            specifics.loyalty_card().loyalty_card_number());
  ASSERT_EQ(card.merchant_domains().size(),
            (size_t)specifics.loyalty_card().merchant_domains().size());
  for (auto [merchant_domain, loyalty_card_domain] :
       base::zip(card.merchant_domains(),
                 specifics.loyalty_card().merchant_domains())) {
    EXPECT_EQ(merchant_domain, loyalty_card_domain);
  }
}

TEST(LoyaltyCardSyncUtilTest, CreateAutofillLoyaltyCardFromSpecifics) {
  EXPECT_EQ(TestLoyaltyCard(), CreateAutofillLoyaltyCardFromSpecifics(
                                   TestLoyaltyCardSpecifics(kId1)));
}

TEST(LoyaltyCardSyncUtilTest, TrimAutofillValuableSpecificsDataForCaching) {
  EXPECT_EQ(TrimAutofillValuableSpecificsDataForCaching(
                TestLoyaltyCardSpecifics(kId1))
                .ByteSizeLong(),
            0u);
}

TEST(VehicleRegistrationSyncUtilTest,
     TrimAutofillValuableSpecificsDataForCaching) {
  sync_pb::AutofillValuableSpecifics specifics;
  specifics.set_id("some_id");
  specifics.set_is_editable(true);
  specifics.mutable_vehicle_registration()->set_vehicle_make("Ford");
  specifics.mutable_vehicle_registration()->set_vehicle_model("Fiesta");
  specifics.mutable_vehicle_registration()->set_vehicle_year("2018");
  specifics.mutable_vehicle_registration()->set_vehicle_identification_number(
      "VIN123");
  specifics.mutable_vehicle_registration()->set_vehicle_license_plate("ABC123");
  specifics.mutable_vehicle_registration()->set_license_plate_region("CA");
  specifics.mutable_vehicle_registration()->set_license_plate_country("US");
  specifics.mutable_vehicle_registration()->set_owner_name("John Doe");

  EXPECT_EQ(
      TrimAutofillValuableSpecificsDataForCaching(specifics).ByteSizeLong(),
      0u);
}

TEST(FlightReservationSyncUtilTest,
     TrimAutofillValuableSpecificsDataForCaching) {
  sync_pb::AutofillValuableSpecifics specifics;
  specifics.set_id("some_id");
  specifics.set_is_editable(true);
  specifics.mutable_flight_reservation()->set_flight_number("BA249");
  specifics.mutable_flight_reservation()->set_flight_ticket_number("12345");
  specifics.mutable_flight_reservation()->set_flight_confirmation_code(
      "ABCDEF");
  specifics.mutable_flight_reservation()->set_passenger_name("Jane Doe");
  specifics.mutable_flight_reservation()->set_departure_airport("LHR");
  specifics.mutable_flight_reservation()->set_arrival_airport("AMS");
  specifics.mutable_flight_reservation()->set_departure_date_unix_epoch_micros(
      123456789);
  specifics.mutable_flight_reservation()->set_arrival_date_unix_epoch_micros(
      987654321);
  specifics.mutable_flight_reservation()->set_airline_logo("logo_url");
  specifics.mutable_flight_reservation()->set_carrier_code("BA");
  specifics.mutable_flight_reservation()
      ->set_departure_airport_utc_offset_seconds(123456789);
  specifics.mutable_flight_reservation()
      ->set_arrival_airport_utc_offset_seconds(987654321);

  EXPECT_EQ(
      TrimAutofillValuableSpecificsDataForCaching(specifics).ByteSizeLong(),
      0u);
}

TEST(EntityInstanceSyncUtilTest, CreateEntityDataFromEntityInstance) {
  EntityInstance vehicle_entity = test::GetVehicleEntityInstance();
  std::unique_ptr<syncer::EntityData> entity_data =
      CreateEntityDataFromEntityInstance(vehicle_entity);

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

}  // namespace autofill
