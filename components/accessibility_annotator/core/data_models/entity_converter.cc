// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/data_models/entity_converter.h"

#include "base/time/time.h"
#include "url/gurl.h"

namespace accessibility_annotator {

namespace {

Date ConvertDate(
    const sync_pb::AccessibilityAnnotationSpecifics::NaiveDate& proto_date) {
  Date date;
  if (proto_date.has_day()) {
    date.day = proto_date.day();
  }
  if (proto_date.has_month()) {
    date.month = proto_date.month();
  }
  if (proto_date.has_year()) {
    date.year = proto_date.year();
  }
  return date;
}

Order ConvertOrder(
    const sync_pb::AccessibilityAnnotationSpecifics::Order& proto_order) {
  Order order;
  if (proto_order.has_order_id()) {
    order.id = proto_order.order_id();
  }
  if (proto_order.has_account()) {
    order.account = proto_order.account();
  }
  if (proto_order.has_order_date()) {
    order.order_date = ConvertDate(proto_order.order_date());
  }
  if (proto_order.has_merchant_name()) {
    order.merchant_name = proto_order.merchant_name();
  }
  if (proto_order.has_merchant_domain()) {
    order.merchant_domain = GURL(proto_order.merchant_domain());
  }
  if (proto_order.has_grand_total()) {
    order.grand_total = proto_order.grand_total();
  }
  for (const auto& product_name : proto_order.product_names()) {
    Order::ItemDescription item;
    item.name = product_name;
    order.products.push_back(std::move(item));
  }
  return order;
}

Shipment ConvertShipment(
    const sync_pb::AccessibilityAnnotationSpecifics::Shipment& proto_shipment) {
  Shipment shipment;
  if (proto_shipment.has_tracking_number()) {
    shipment.tracking_number = proto_shipment.tracking_number();
  }
  if (proto_shipment.associated_order_ids_size() > 0) {
    shipment.associated_order_id = proto_shipment.associated_order_ids(0);
  }
  if (proto_shipment.has_delivery_address()) {
    shipment.delivery_address = proto_shipment.delivery_address();
  }
  if (proto_shipment.has_carrier_name()) {
    shipment.carrier_name = proto_shipment.carrier_name();
  }
  if (proto_shipment.has_carrier_domain()) {
    shipment.carrier_domain = GURL(proto_shipment.carrier_domain());
  }
  if (proto_shipment.has_estimated_delivery_date()) {
    shipment.estimated_delivery_date =
        ConvertDate(proto_shipment.estimated_delivery_date());
  }
  return shipment;
}

DriverLicense ConvertDriverLicense(
    const sync_pb::AccessibilityAnnotationSpecifics::DriversLicense& proto_dl) {
  DriverLicense dl;
  if (proto_dl.has_name()) {
    dl.name = proto_dl.name();
  }
  if (proto_dl.has_number()) {
    dl.number = proto_dl.number();
  }
  if (proto_dl.has_expiration_date()) {
    dl.expiration_date = ConvertDate(proto_dl.expiration_date());
  }
  if (proto_dl.has_issue_date()) {
    dl.issue_date = ConvertDate(proto_dl.issue_date());
  }
  if (proto_dl.has_state()) {
    dl.state = proto_dl.state();
  }
  // country is in entity.h but not in proto.
  return dl;
}

Passport ConvertPassport(
    const sync_pb::AccessibilityAnnotationSpecifics::Passport& proto_passport) {
  Passport passport;
  if (proto_passport.has_name()) {
    passport.name = proto_passport.name();
  }
  if (proto_passport.has_number()) {
    passport.number = proto_passport.number();
  }
  if (proto_passport.has_expiration_date()) {
    passport.expiration_date = ConvertDate(proto_passport.expiration_date());
  }
  if (proto_passport.has_issue_date()) {
    passport.issue_date = ConvertDate(proto_passport.issue_date());
  }
  if (proto_passport.has_issuing_country()) {
    passport.issuing_country = proto_passport.issuing_country();
  }
  return passport;
}

NationalId ConvertNationalId(
    const sync_pb::AccessibilityAnnotationSpecifics::NationalId&
        proto_national_id) {
  NationalId national_id;
  if (proto_national_id.has_name()) {
    national_id.name = proto_national_id.name();
  }
  if (proto_national_id.has_number()) {
    national_id.number = proto_national_id.number();
  }
  if (proto_national_id.has_expiration_date()) {
    national_id.expiration_date =
        ConvertDate(proto_national_id.expiration_date());
  }
  if (proto_national_id.has_issue_date()) {
    national_id.issue_date = ConvertDate(proto_national_id.issue_date());
  }
  if (proto_national_id.has_issuing_country()) {
    national_id.issuing_country = proto_national_id.issuing_country();
  }
  return national_id;
}

Flight ConvertFlight(
    const sync_pb::AccessibilityAnnotationSpecifics::FlightReservation&
        proto_flight) {
  Flight flight;
  if (proto_flight.has_flight_number()) {
    flight.flight_number = proto_flight.flight_number();
  }
  if (proto_flight.has_flight_ticket_number()) {
    flight.ticket_number = proto_flight.flight_ticket_number();
  }
  if (proto_flight.has_flight_confirmation_code()) {
    flight.confirmation_code = proto_flight.flight_confirmation_code();
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
  if (proto_flight.has_departure_date_unix_epoch_seconds()) {
    flight.departure_date = base::Time::FromSecondsSinceUnixEpoch(
        proto_flight.departure_date_unix_epoch_seconds());
  }
  return flight;
}

Vehicle ConvertVehicle(
    const sync_pb::AccessibilityAnnotationSpecifics::Vehicle& proto_vehicle) {
  Vehicle vehicle;
  if (proto_vehicle.has_vehicle_make()) {
    vehicle.make = proto_vehicle.vehicle_make();
  }
  if (proto_vehicle.has_vehicle_model()) {
    vehicle.model = proto_vehicle.vehicle_model();
  }
  if (proto_vehicle.has_vehicle_year()) {
    vehicle.year = proto_vehicle.vehicle_year();
  }
  if (proto_vehicle.has_vehicle_identification_number()) {
    vehicle.vin = proto_vehicle.vehicle_identification_number();
  }
  if (proto_vehicle.has_vehicle_license_plate()) {
    vehicle.plate_number = proto_vehicle.vehicle_license_plate();
  }
  if (proto_vehicle.has_license_plate_region()) {
    vehicle.plate_state = proto_vehicle.license_plate_region();
  }
  if (proto_vehicle.has_owner_name()) {
    vehicle.owner = proto_vehicle.owner_name();
  }
  return vehicle;
}

}  // namespace

std::optional<Entity> CreateEntityFromSpecifics(
    const sync_pb::AccessibilityAnnotationSpecifics& specifics) {
  if (!specifics.has_id()) {
    return std::nullopt;
  }
  Entity entity;
  entity.entity_id = specifics.id();

  switch (specifics.entity_case()) {
    case sync_pb::AccessibilityAnnotationSpecifics::kOrder:
      entity.specifics = ConvertOrder(specifics.order());
      break;
    case sync_pb::AccessibilityAnnotationSpecifics::kShipment:
      entity.specifics = ConvertShipment(specifics.shipment());
      break;
    case sync_pb::AccessibilityAnnotationSpecifics::kDriversLicense:
      entity.specifics = ConvertDriverLicense(specifics.drivers_license());
      break;
    case sync_pb::AccessibilityAnnotationSpecifics::kPassport:
      entity.specifics = ConvertPassport(specifics.passport());
      break;
    case sync_pb::AccessibilityAnnotationSpecifics::kNationalId:
      entity.specifics = ConvertNationalId(specifics.national_id());
      break;
    case sync_pb::AccessibilityAnnotationSpecifics::kFlightReservation:
      entity.specifics = ConvertFlight(specifics.flight_reservation());
      break;
    case sync_pb::AccessibilityAnnotationSpecifics::kVehicle:
      entity.specifics = ConvertVehicle(specifics.vehicle());
      break;
    case sync_pb::AccessibilityAnnotationSpecifics::ENTITY_NOT_SET:
      return std::nullopt;
  }

  return entity;
}

}  // namespace accessibility_annotator
