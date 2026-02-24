// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/data_models/entity.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace accessibility_annotator {

TEST(EntityTest, Flight) {
  Entity entity;
  Flight& flight = entity.specifics.emplace<Flight>();
  flight.flight_number = "UA123";
  flight.departure_airport = "SFO";
  flight.arrival_airport = "JFK";
  base::Time departure;
  ASSERT_TRUE(base::Time::FromString("2026-02-10T11:47:48Z", &departure));
  flight.departure_date = departure;

  ASSERT_EQ(entity.GetType(), EntityType::kFlight);
  EXPECT_EQ(flight.flight_number, "UA123");
  EXPECT_EQ(flight.departure_airport, "SFO");
  EXPECT_EQ(flight.arrival_airport, "JFK");
  EXPECT_EQ(flight.departure_date, departure);
}

TEST(EntityTest, Order) {
  Entity entity;
  Order& order = entity.specifics.emplace<Order>();
  order.id = "ORDER123";
  order.account = "test_user@test.com";
  base::Time order_time;
  ASSERT_TRUE(base::Time::FromString("2026-02-10T11:50:00Z", &order_time));
  order.order_date = order_time;
  order.merchant_name = "Google Store";
  order.merchant_domain = GURL("https://store.google.com");
  order.grand_total = "$100.00";
  order.products.emplace_back();
  order.products.back().name = "Test Product";
  order.products.back().quantity = 1;
  order.products.back().description = "A product for testing.";

  ASSERT_EQ(entity.GetType(), EntityType::kOrder);
  EXPECT_EQ(order.id, "ORDER123");
  EXPECT_EQ(order.account, "test_user@test.com");
  EXPECT_EQ(order.order_date, order_time);
  EXPECT_EQ(order.merchant_name, "Google Store");
  EXPECT_EQ(order.merchant_domain, GURL("https://store.google.com"));
  EXPECT_EQ(order.grand_total, "$100.00");
  ASSERT_EQ(order.products.size(), 1u);
  EXPECT_EQ(order.products[0].name, "Test Product");
  EXPECT_EQ(order.products[0].quantity, 1);
  EXPECT_EQ(order.products[0].description, "A product for testing.");
}

TEST(EntityTest, Shipment) {
  Entity entity;
  Shipment& shipment = entity.specifics.emplace<Shipment>();
  shipment.tracking_number = "TRACK123";
  shipment.associated_order_id = "ORDER123";
  shipment.delivery_address = "1600 Amphitheatre Parkway, Mountain View, CA";
  shipment.carrier_name = "UPS";
  shipment.carrier_domain = GURL("https://ups.com");
  base::Time delivery_date;
  ASSERT_TRUE(base::Time::FromString("2026-02-15T10:00:00Z", &delivery_date));
  shipment.estimated_delivery_date = delivery_date;

  ASSERT_EQ(entity.GetType(), EntityType::kShipment);
  EXPECT_EQ(shipment.tracking_number, "TRACK123");
  EXPECT_EQ(shipment.associated_order_id, "ORDER123");
  EXPECT_EQ(shipment.delivery_address,
            "1600 Amphitheatre Parkway, Mountain View, CA");
  EXPECT_EQ(shipment.carrier_name, "UPS");
  EXPECT_EQ(shipment.carrier_domain, GURL("https://ups.com"));
  EXPECT_EQ(shipment.estimated_delivery_date, delivery_date);
}

TEST(EntityTest, DriverLicense) {
  Entity entity;
  DriverLicense& license = entity.specifics.emplace<DriverLicense>();
  license.name = "John Doe";
  license.number = "12345";
  base::Time expiration_date, issue_date;
  ASSERT_TRUE(base::Time::FromString("2030-01-01T00:00:00Z", &expiration_date));
  ASSERT_TRUE(base::Time::FromString("2020-01-01T00:00:00Z", &issue_date));
  license.expiration_date = expiration_date;
  license.issue_date = issue_date;
  license.country = "USA";
  license.region = "CA";

  ASSERT_EQ(entity.GetType(), EntityType::kDriverLicense);
  EXPECT_EQ(license.name, "John Doe");
  EXPECT_EQ(license.number, "12345");
  EXPECT_EQ(license.expiration_date, expiration_date);
  EXPECT_EQ(license.issue_date, issue_date);
  EXPECT_EQ(license.country, "USA");
  EXPECT_EQ(license.region, "CA");
}

TEST(EntityTest, Passport) {
  Entity entity;
  Passport& passport = entity.specifics.emplace<Passport>();
  passport.name = "Jane Doe";
  passport.number = "67890";
  base::Time expiration_date, issue_date;
  ASSERT_TRUE(base::Time::FromString("2032-01-01T00:00:00Z", &expiration_date));
  ASSERT_TRUE(base::Time::FromString("2022-01-01T00:00:00Z", &issue_date));
  passport.expiration_date = expiration_date;
  passport.issue_date = issue_date;
  passport.issuing_country = "USA";

  ASSERT_EQ(entity.GetType(), EntityType::kPassport);
  EXPECT_EQ(passport.name, "Jane Doe");
  EXPECT_EQ(passport.number, "67890");
  EXPECT_EQ(passport.expiration_date, expiration_date);
  EXPECT_EQ(passport.issue_date, issue_date);
  EXPECT_EQ(passport.issuing_country, "USA");
}

TEST(EntityTest, NationalId) {
  Entity entity;
  NationalId& national_id = entity.specifics.emplace<NationalId>();
  national_id.name = "Sam Smith";
  national_id.number = "ABCDE";
  base::Time expiration_date, issue_date;
  ASSERT_TRUE(base::Time::FromString("2028-01-01T00:00:00Z", &expiration_date));
  ASSERT_TRUE(base::Time::FromString("2018-01-01T00:00:00Z", &issue_date));
  national_id.expiration_date = expiration_date;
  national_id.issue_date = issue_date;
  national_id.issuing_country = "USA";

  ASSERT_EQ(entity.GetType(), EntityType::kNationalId);
  EXPECT_EQ(national_id.name, "Sam Smith");
  EXPECT_EQ(national_id.number, "ABCDE");
  EXPECT_EQ(national_id.expiration_date, expiration_date);
  EXPECT_EQ(national_id.issue_date, issue_date);
  EXPECT_EQ(national_id.issuing_country, "USA");
}

}  // namespace accessibility_annotator
