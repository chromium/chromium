// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/ingested_content_annotation_utils.h"

#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/accessibility_annotator/core/content_annotator/content_annotations_data.h"
#include "components/accessibility_annotator/core/data_models/entity.h"
#include "components/accessibility_annotator/core/data_models/entity_types.h"
#include "components/optimization_guide/proto/features/content_annotation.pb.h"

namespace accessibility_annotator {

namespace {

Date ConvertDate(const optimization_guide::proto::Date& proto_date) {
  Date date;
  if (proto_date.has_year()) {
    date.year = proto_date.year();
  }
  if (proto_date.has_month()) {
    date.month = proto_date.month();
  }
  if (proto_date.has_day()) {
    date.day = proto_date.day();
  }
  return date;
}

base::Time ConvertDateToTime(
    const optimization_guide::proto::Date& proto_date) {
  if (!proto_date.has_year() || !proto_date.has_month() ||
      !proto_date.has_day()) {
    return base::Time();
  }

  base::Time::Exploded exploded = {.year = proto_date.year(),
                                   .month = proto_date.month(),
                                   .day_of_month = proto_date.day()};
  base::Time time;
  if (base::Time::FromUTCExploded(exploded, &time)) {
    return time;
  }
  return base::Time();
}

Order ConvertOrder(const optimization_guide::proto::Order& proto_order) {
  Order order;
  if (proto_order.has_id()) {
    order.id = proto_order.id();
  }
  if (proto_order.has_order_date()) {
    order.order_date = ConvertDate(proto_order.order_date());
  }
  if (proto_order.has_grand_total()) {
    order.grand_total = base::NumberToString(proto_order.grand_total());
  }
  for (const auto& proto_product : proto_order.products()) {
    Order::ItemDescription product;
    if (proto_product.has_name()) {
      product.name = proto_product.name();
    }
    if (proto_product.has_quantity()) {
      product.quantity = proto_product.quantity();
    }
    order.products.push_back(std::move(product));
  }
  return order;
}

Shipment ConvertShipment(
    const optimization_guide::proto::Shipment& proto_shipment) {
  Shipment shipment;
  if (proto_shipment.has_associated_order_id()) {
    shipment.associated_order_id = proto_shipment.associated_order_id();
  }
  if (proto_shipment.has_tracking_number()) {
    shipment.tracking_number = proto_shipment.tracking_number();
  }
  if (proto_shipment.has_carrier_name()) {
    shipment.carrier_name = proto_shipment.carrier_name();
  }
  if (proto_shipment.has_delivery_address()) {
    shipment.delivery_address = proto_shipment.delivery_address();
  }
  return shipment;
}

FlightReservation ConvertFlightReservation(
    const optimization_guide::proto::FlightReservation& proto_flight) {
  FlightReservation flight;
  if (proto_flight.has_confirmation_code()) {
    flight.confirmation_code = proto_flight.confirmation_code();
  }
  if (proto_flight.has_flight_number()) {
    flight.flight_number = proto_flight.flight_number();
  }
  if (proto_flight.has_passenger_name()) {
    flight.passenger_name = proto_flight.passenger_name();
  }
  if (proto_flight.has_departure_airport()) {
    flight.departure_airport = proto_flight.departure_airport();
  }
  if (proto_flight.has_arrival_airport()) {
    flight.arrival_airport = proto_flight.arrival_airport();
  }
  if (proto_flight.has_departure_date()) {
    flight.departure_date = ConvertDateToTime(proto_flight.departure_date());
  }
  return flight;
}

}  // namespace

IngestedContentAnnotation ConvertIngestedContentAnnotation(
    history::VisitID visit_id,
    const ContentAnnotationsData& content_annotation_data) {
  if (!content_annotation_data.content_annotation.has_structured_data()) {
    return IngestedContentAnnotation();
  }

  const auto& structured_data =
      content_annotation_data.content_annotation.structured_data();

  std::vector<Entity> structured_entities;
  structured_entities.reserve(structured_data.orders_size() +
                              structured_data.shipments_size() +
                              structured_data.flight_reservations_size());

  // TODO(crbug.com/502225870): Add id and sources to the entity.
  // TODO(crbug.com/502225870): Add support for other entity types.
  for (const auto& proto_order : structured_data.orders()) {
    Entity entity;
    entity.specifics = ConvertOrder(proto_order);
    structured_entities.push_back(std::move(entity));
  }

  for (const auto& proto_shipment : structured_data.shipments()) {
    Entity entity;
    entity.specifics = ConvertShipment(proto_shipment);
    structured_entities.push_back(std::move(entity));
  }

  for (const auto& proto_flight : structured_data.flight_reservations()) {
    Entity entity;
    entity.specifics = ConvertFlightReservation(proto_flight);
    structured_entities.push_back(std::move(entity));
  }

  IngestedContentAnnotation::AnnotationStatus status;
  switch (content_annotation_data.content_annotation.status()) {
    case optimization_guide::proto::ContentAnnotation::CONFIRMED:
      status = IngestedContentAnnotation::AnnotationStatus::kConfirmed;
      break;
    case optimization_guide::proto::ContentAnnotation::PENDING:
      status = IngestedContentAnnotation::AnnotationStatus::kPending;
      break;
    default:
      status = IngestedContentAnnotation::AnnotationStatus::kUnknown;
      break;
  }

  std::vector<optimization_guide::proto::DynamicAttribute> supplemental_data(
      content_annotation_data.content_annotation.supplemental_data().begin(),
      content_annotation_data.content_annotation.supplemental_data().end());

  return IngestedContentAnnotation(
      base::NumberToString(visit_id), content_annotation_data.url,
      content_annotation_data.navigation_timestamp,
      content_annotation_data.content_annotation.description(), status,
      std::move(structured_entities), std::move(supplemental_data));
}

}  // namespace accessibility_annotator
