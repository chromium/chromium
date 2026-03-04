// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/data_models/entity_converter.h"

#include "base/time/time.h"
#include "components/accessibility_annotator/core/data_models/entity.h"
#include "components/sync/protocol/accessibility_annotation_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace accessibility_annotator {
namespace {

TEST(EntityConverterTest, ConvertOrder) {
  sync_pb::AccessibilityAnnotationSpecifics specifics;
  specifics.set_id("entity_id");
  auto* order = specifics.mutable_order();
  order->set_order_id("order_123");
  order->set_account("user@example.com");
  auto* order_date = order->mutable_order_date();
  order_date->set_year(2023);
  order_date->set_month(11);
  order_date->set_day(15);
  order->set_merchant_name("Merchant Inc");
  order->set_merchant_domain("https://merchant.com");
  order->add_product_names("Widget A");
  order->add_product_names("Widget B");
  order->set_grand_total("$50.00");

  std::optional<Entity> result = CreateEntityFromSpecifics(specifics);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->entity_id, "entity_id");
  ASSERT_TRUE(std::holds_alternative<Order>(result->specifics));

  const Order& entity_order = std::get<Order>(result->specifics);
  EXPECT_EQ(entity_order.id, "order_123");
  EXPECT_EQ(entity_order.account, "user@example.com");
  ASSERT_TRUE(entity_order.order_date.has_value());
  EXPECT_EQ(entity_order.order_date->year, 2023);
  EXPECT_EQ(entity_order.order_date->month, 11);
  EXPECT_EQ(entity_order.order_date->day, 15);
  EXPECT_EQ(entity_order.merchant_name, "Merchant Inc");
  EXPECT_EQ(entity_order.merchant_domain, GURL("https://merchant.com"));
  EXPECT_EQ(entity_order.grand_total, "$50.00");

  ASSERT_EQ(entity_order.products.size(), 2u);
  EXPECT_EQ(entity_order.products[0].name, "Widget A");
  EXPECT_EQ(entity_order.products[1].name, "Widget B");
}

TEST(EntityConverterTest, ConvertShipment) {
  sync_pb::AccessibilityAnnotationSpecifics specifics;
  specifics.set_id("shipment_id");
  auto* shipment = specifics.mutable_shipment();
  shipment->set_tracking_number("track_123");
  shipment->add_associated_order_ids("order_1");
  shipment->add_associated_order_ids("order_2");
  shipment->set_delivery_address("123 Main St");
  shipment->set_carrier_name("Carrier Co");
  shipment->set_carrier_domain("https://carrier.com");
  auto* delivery_date = shipment->mutable_estimated_delivery_date();
  delivery_date->set_year(2024);
  delivery_date->set_month(3);
  delivery_date->set_day(10);

  std::optional<Entity> result = CreateEntityFromSpecifics(specifics);

  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(std::holds_alternative<Shipment>(result->specifics));

  const Shipment& entity_shipment = std::get<Shipment>(result->specifics);
  EXPECT_EQ(entity_shipment.tracking_number, "track_123");
  EXPECT_EQ(entity_shipment.associated_order_id, "order_1");
  EXPECT_EQ(entity_shipment.delivery_address, "123 Main St");
  EXPECT_EQ(entity_shipment.carrier_name, "Carrier Co");
  EXPECT_EQ(entity_shipment.carrier_domain, GURL("https://carrier.com"));
  ASSERT_TRUE(entity_shipment.estimated_delivery_date.has_value());
  EXPECT_EQ(entity_shipment.estimated_delivery_date->year, 2024);
  EXPECT_EQ(entity_shipment.estimated_delivery_date->month, 3);
  EXPECT_EQ(entity_shipment.estimated_delivery_date->day, 10);
}

TEST(EntityConverterTest, ConvertDriverLicense) {
  sync_pb::AccessibilityAnnotationSpecifics specifics;
  specifics.set_id("dl_id");
  auto* dl = specifics.mutable_drivers_license();
  dl->set_name("John Doe");
  dl->set_number("DL123456");
  auto* exp_date = dl->mutable_expiration_date();
  exp_date->set_year(2030);
  exp_date->set_month(1);
  exp_date->set_day(1);
  auto* issue_date = dl->mutable_issue_date();
  issue_date->set_year(2020);
  issue_date->set_month(1);
  issue_date->set_day(1);
  dl->set_state("CA");

  std::optional<Entity> result = CreateEntityFromSpecifics(specifics);

  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(std::holds_alternative<DriverLicense>(result->specifics));

  const DriverLicense& entity_dl = std::get<DriverLicense>(result->specifics);
  EXPECT_EQ(entity_dl.name, "John Doe");
  EXPECT_EQ(entity_dl.number, "DL123456");
  ASSERT_TRUE(entity_dl.expiration_date.has_value());
  EXPECT_EQ(entity_dl.expiration_date->year, 2030);
  EXPECT_EQ(entity_dl.expiration_date->month, 1);
  EXPECT_EQ(entity_dl.expiration_date->day, 1);
  ASSERT_TRUE(entity_dl.issue_date.has_value());
  EXPECT_EQ(entity_dl.issue_date->year, 2020);
  EXPECT_EQ(entity_dl.issue_date->month, 1);
  EXPECT_EQ(entity_dl.issue_date->day, 1);
  EXPECT_EQ(entity_dl.state, "CA");
}

TEST(EntityConverterTest, ConvertPassport) {
  sync_pb::AccessibilityAnnotationSpecifics specifics;
  specifics.set_id("passport_id");
  auto* passport = specifics.mutable_passport();
  passport->set_name("Jane Doe");
  passport->set_number("P12345678");
  auto* exp_date = passport->mutable_expiration_date();
  exp_date->set_year(2032);
  exp_date->set_month(5);
  exp_date->set_day(20);
  auto* issue_date = passport->mutable_issue_date();
  issue_date->set_year(2022);
  issue_date->set_month(5);
  issue_date->set_day(20);
  passport->set_issuing_country("US");

  std::optional<Entity> result = CreateEntityFromSpecifics(specifics);

  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(std::holds_alternative<Passport>(result->specifics));

  const Passport& entity_passport = std::get<Passport>(result->specifics);
  EXPECT_EQ(entity_passport.name, "Jane Doe");
  EXPECT_EQ(entity_passport.number, "P12345678");
  ASSERT_TRUE(entity_passport.expiration_date.has_value());
  EXPECT_EQ(entity_passport.expiration_date->year, 2032);
  EXPECT_EQ(entity_passport.expiration_date->month, 5);
  EXPECT_EQ(entity_passport.expiration_date->day, 20);
  ASSERT_TRUE(entity_passport.issue_date.has_value());
  EXPECT_EQ(entity_passport.issue_date->year, 2022);
  EXPECT_EQ(entity_passport.issue_date->month, 5);
  EXPECT_EQ(entity_passport.issue_date->day, 20);
  EXPECT_EQ(entity_passport.issuing_country, "US");
}

TEST(EntityConverterTest, ConvertNationalId) {
  sync_pb::AccessibilityAnnotationSpecifics specifics;
  specifics.set_id("nid_id");
  auto* nid = specifics.mutable_national_id();
  nid->set_name("Citizen X");
  nid->set_number("ID98765");
  auto* exp_date = nid->mutable_expiration_date();
  exp_date->set_year(2028);
  exp_date->set_month(12);
  exp_date->set_day(31);
  auto* issue_date = nid->mutable_issue_date();
  issue_date->set_year(2018);
  issue_date->set_month(1);
  issue_date->set_day(1);
  nid->set_issuing_country("FR");

  std::optional<Entity> result = CreateEntityFromSpecifics(specifics);

  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(std::holds_alternative<NationalId>(result->specifics));

  const NationalId& entity_nid = std::get<NationalId>(result->specifics);
  EXPECT_EQ(entity_nid.name, "Citizen X");
  EXPECT_EQ(entity_nid.number, "ID98765");
  ASSERT_TRUE(entity_nid.expiration_date.has_value());
  EXPECT_EQ(entity_nid.expiration_date->year, 2028);
  EXPECT_EQ(entity_nid.expiration_date->month, 12);
  EXPECT_EQ(entity_nid.expiration_date->day, 31);
  ASSERT_TRUE(entity_nid.issue_date.has_value());
  EXPECT_EQ(entity_nid.issue_date->year, 2018);
  EXPECT_EQ(entity_nid.issue_date->month, 1);
  EXPECT_EQ(entity_nid.issue_date->day, 1);
  EXPECT_EQ(entity_nid.issuing_country, "FR");
}

TEST(EntityConverterTest, ConvertFlight) {
  sync_pb::AccessibilityAnnotationSpecifics specifics;
  specifics.set_id("flight_id");
  auto* flight = specifics.mutable_flight_reservation();
  flight->set_flight_number("UA100");
  flight->set_flight_ticket_number("TKT123");
  flight->set_flight_confirmation_code("CONF456");
  flight->set_passenger_name("Passenger P");
  flight->set_departure_airport("SFO");
  flight->set_arrival_airport("JFK");
  flight->set_departure_date_unix_epoch_seconds(1750000000);

  std::optional<Entity> result = CreateEntityFromSpecifics(specifics);

  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(std::holds_alternative<Flight>(result->specifics));

  const Flight& entity_flight = std::get<Flight>(result->specifics);
  EXPECT_EQ(entity_flight.flight_number, "UA100");
  EXPECT_EQ(entity_flight.ticket_number, "TKT123");
  EXPECT_EQ(entity_flight.confirmation_code, "CONF456");
  EXPECT_EQ(entity_flight.passenger_name, "Passenger P");
  EXPECT_EQ(entity_flight.departure_airport, "SFO");
  EXPECT_EQ(entity_flight.arrival_airport, "JFK");
  EXPECT_EQ(entity_flight.departure_date,
            base::Time::FromSecondsSinceUnixEpoch(1750000000));
}

TEST(EntityConverterTest, ConvertVehicle) {
  sync_pb::AccessibilityAnnotationSpecifics specifics;
  specifics.set_id("vehicle_id");
  auto* vehicle = specifics.mutable_vehicle();
  vehicle->set_vehicle_make("Toyota");
  vehicle->set_vehicle_model("Camry");
  vehicle->set_vehicle_year("2022");
  vehicle->set_vehicle_identification_number("VIN123456789");
  vehicle->set_vehicle_license_plate("ABC-123");
  vehicle->set_license_plate_region("CA");
  vehicle->set_owner_name("Owner Name");

  std::optional<Entity> result = CreateEntityFromSpecifics(specifics);

  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(std::holds_alternative<Vehicle>(result->specifics));

  const Vehicle& entity_vehicle = std::get<Vehicle>(result->specifics);
  EXPECT_EQ(entity_vehicle.make, "Toyota");
  EXPECT_EQ(entity_vehicle.model, "Camry");
  EXPECT_EQ(entity_vehicle.year, "2022");
  EXPECT_EQ(entity_vehicle.vin, "VIN123456789");
  EXPECT_EQ(entity_vehicle.plate_number, "ABC-123");
  EXPECT_EQ(entity_vehicle.plate_state, "CA");
  EXPECT_EQ(entity_vehicle.owner, "Owner Name");
}

TEST(EntityConverterTest, MissingId) {
  sync_pb::AccessibilityAnnotationSpecifics specifics;
  specifics.mutable_order()->set_order_id("order_123");
  // No ID set on specifics.

  std::optional<Entity> result = CreateEntityFromSpecifics(specifics);

  EXPECT_FALSE(result.has_value());
}

}  // namespace
}  // namespace accessibility_annotator
