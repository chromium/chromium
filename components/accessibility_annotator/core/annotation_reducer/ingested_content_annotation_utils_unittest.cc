// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/ingested_content_annotation_utils.h"

#include "base/time/time.h"
#include "components/accessibility_annotator/core/annotation_reducer/ingested_content_annotation.h"
#include "components/accessibility_annotator/core/content_annotator/content_annotations_data.h"
#include "components/accessibility_annotator/core/data_models/entity.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"
#include "components/optimization_guide/proto/features/content_annotation.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace accessibility_annotator {
namespace {

TEST(IngestedContentAnnotationUtilsTest, ConvertOrder) {
  ContentAnnotationsData annotation_data;
  annotation_data.url = GURL("https://example.com");
  annotation_data.navigation_timestamp =
      base::Time::FromSecondsSinceUnixEpoch(1600000000);

  auto* content_annotation = &annotation_data.content_annotation;
  content_annotation->set_description("Test Order Description");
  content_annotation->set_status(
      optimization_guide::proto::ContentAnnotation::CONFIRMED);

  auto* structured_data = content_annotation->mutable_structured_data();
  auto* order = structured_data->add_orders();
  order->set_id("order_123");
  auto* order_date = order->mutable_order_date();
  order_date->set_year(2023);
  order_date->set_month(11);
  order_date->set_day(15);
  order->set_grand_total(50.23);

  auto* product1 = order->add_products();
  product1->set_name("Widget A");
  product1->set_quantity(1);

  auto* product2 = order->add_products();
  product2->set_name("Widget B");
  product2->set_quantity(2);

  auto ingested_annotation =
      ConvertIngestedContentAnnotation(123, annotation_data);

  EXPECT_EQ(ingested_annotation.id, "123");
  EXPECT_EQ(ingested_annotation.url, GURL("https://example.com"));
  EXPECT_EQ(ingested_annotation.timestamp,
            base::Time::FromSecondsSinceUnixEpoch(1600000000));
  EXPECT_EQ(ingested_annotation.description, "Test Order Description");
  EXPECT_EQ(ingested_annotation.status,
            IngestedContentAnnotation::AnnotationStatus::kConfirmed);

  ASSERT_EQ(ingested_annotation.structured_entities.size(), 1u);
  const Entity& entity = ingested_annotation.structured_entities[0];
  ASSERT_TRUE(std::holds_alternative<Order>(entity.specifics));

  const Order& entity_order = std::get<Order>(entity.specifics);
  EXPECT_EQ(entity_order.id, "order_123");
  ASSERT_TRUE(entity_order.order_date.has_value());
  EXPECT_EQ(entity_order.order_date->year, 2023);
  EXPECT_EQ(entity_order.order_date->month, 11);
  EXPECT_EQ(entity_order.order_date->day, 15);
  EXPECT_EQ(entity_order.grand_total, "50.23");

  ASSERT_EQ(entity_order.products.size(), 2u);
  EXPECT_EQ(entity_order.products[0].name, "Widget A");
  EXPECT_EQ(entity_order.products[0].quantity, 1);
  EXPECT_EQ(entity_order.products[1].name, "Widget B");
  EXPECT_EQ(entity_order.products[1].quantity, 2);
}

TEST(IngestedContentAnnotationUtilsTest, ConvertShipment) {
  ContentAnnotationsData annotation_data;
  annotation_data.url = GURL("https://example.com/shipment");
  annotation_data.navigation_timestamp =
      base::Time::FromSecondsSinceUnixEpoch(1610000000);

  auto* content_annotation = &annotation_data.content_annotation;
  content_annotation->set_description("Test Shipment Description");
  content_annotation->set_status(
      optimization_guide::proto::ContentAnnotation::PENDING);

  auto* structured_data = content_annotation->mutable_structured_data();
  auto* shipment = structured_data->add_shipments();
  shipment->set_associated_order_id("order_456");
  shipment->set_tracking_number("track_456");
  shipment->set_carrier_name("Carrier X");
  shipment->set_delivery_address("456 ABC St");

  auto ingested_annotation =
      ConvertIngestedContentAnnotation(456, annotation_data);

  EXPECT_EQ(ingested_annotation.id, "456");
  EXPECT_EQ(ingested_annotation.url, GURL("https://example.com/shipment"));
  EXPECT_EQ(ingested_annotation.timestamp,
            base::Time::FromSecondsSinceUnixEpoch(1610000000));
  EXPECT_EQ(ingested_annotation.description, "Test Shipment Description");
  EXPECT_EQ(ingested_annotation.status,
            IngestedContentAnnotation::AnnotationStatus::kPending);

  ASSERT_EQ(ingested_annotation.structured_entities.size(), 1u);
  const Entity& entity = ingested_annotation.structured_entities[0];
  ASSERT_TRUE(std::holds_alternative<Shipment>(entity.specifics));

  const Shipment& entity_shipment = std::get<Shipment>(entity.specifics);
  EXPECT_EQ(entity_shipment.associated_order_id, "order_456");
  EXPECT_EQ(entity_shipment.tracking_number, "track_456");
  EXPECT_EQ(entity_shipment.carrier_name, "Carrier X");
  EXPECT_EQ(entity_shipment.delivery_address, "456 ABC St");
}

TEST(IngestedContentAnnotationUtilsTest, ConvertFlightReservation) {
  ContentAnnotationsData annotation_data;
  annotation_data.url = GURL("https://example.com/flight");
  annotation_data.navigation_timestamp =
      base::Time::FromSecondsSinceUnixEpoch(1620000000);

  auto* content_annotation = &annotation_data.content_annotation;
  content_annotation->set_description("Test Flight Description");
  content_annotation->set_status(
      optimization_guide::proto::ContentAnnotation::STATUS_UNSPECIFIED);

  auto* structured_data = content_annotation->mutable_structured_data();
  auto* flight = structured_data->add_flight_reservations();
  flight->set_confirmation_code("CONF789");
  flight->set_flight_number("UA200");
  flight->set_passenger_name("ABC");
  flight->set_departure_airport("LAX");
  flight->set_arrival_airport("ORD");

  auto* dep_date = flight->mutable_departure_date();
  dep_date->set_year(2024);
  dep_date->set_month(5);
  dep_date->set_day(20);

  auto ingested_annotation =
      ConvertIngestedContentAnnotation(789, annotation_data);

  EXPECT_EQ(ingested_annotation.id, "789");
  EXPECT_EQ(ingested_annotation.url, GURL("https://example.com/flight"));
  EXPECT_EQ(ingested_annotation.timestamp,
            base::Time::FromSecondsSinceUnixEpoch(1620000000));
  EXPECT_EQ(ingested_annotation.description, "Test Flight Description");
  EXPECT_EQ(ingested_annotation.status,
            IngestedContentAnnotation::AnnotationStatus::kUnknown);

  ASSERT_EQ(ingested_annotation.structured_entities.size(), 1u);
  const Entity& entity = ingested_annotation.structured_entities[0];
  ASSERT_TRUE(std::holds_alternative<FlightReservation>(entity.specifics));

  const FlightReservation& entity_flight =
      std::get<FlightReservation>(entity.specifics);
  EXPECT_EQ(entity_flight.confirmation_code, "CONF789");
  EXPECT_EQ(entity_flight.flight_number, "UA200");
  EXPECT_EQ(entity_flight.passenger_name, "ABC");
  EXPECT_EQ(entity_flight.departure_airport, "LAX");
  EXPECT_EQ(entity_flight.arrival_airport, "ORD");

  base::Time::Exploded exploded;
  exploded.year = 2024;
  exploded.month = 5;
  exploded.day_of_week = 0;
  exploded.day_of_month = 20;
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;
  base::Time expected_time;
  EXPECT_TRUE(base::Time::FromUTCExploded(exploded, &expected_time));
  EXPECT_EQ(entity_flight.departure_date, expected_time);
}

TEST(IngestedContentAnnotationUtilsTest, ConvertEmptyStructuredData) {
  ContentAnnotationsData annotation_data;
  auto ingested_annotation =
      ConvertIngestedContentAnnotation(999, annotation_data);

  EXPECT_TRUE(ingested_annotation.id.empty());
  EXPECT_TRUE(ingested_annotation.structured_entities.empty());
}

}  // namespace
}  // namespace accessibility_annotator
