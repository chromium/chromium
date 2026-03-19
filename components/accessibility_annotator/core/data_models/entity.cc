// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/data_models/entity.h"

#include <variant>

#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace accessibility_annotator {

FlightReservation::FlightReservation() = default;
FlightReservation::FlightReservation(const FlightReservation& other) = default;
FlightReservation::FlightReservation(FlightReservation&& other) = default;
FlightReservation& FlightReservation::operator=(
    const FlightReservation& other) = default;
FlightReservation& FlightReservation::operator=(FlightReservation&& other) =
    default;
FlightReservation::~FlightReservation() = default;

Order::ItemDescription::ItemDescription() = default;
Order::ItemDescription::ItemDescription(const ItemDescription& other) = default;
Order::ItemDescription::ItemDescription(ItemDescription&& other) = default;
Order::ItemDescription& Order::ItemDescription::operator=(
    const ItemDescription& other) = default;
Order::ItemDescription& Order::ItemDescription::operator=(
    ItemDescription&& other) = default;
Order::ItemDescription::~ItemDescription() = default;

Order::Order() = default;
Order::Order(const Order& other) = default;
Order::Order(Order&& other) = default;
Order& Order::operator=(const Order& other) = default;
Order& Order::operator=(Order&& other) = default;
Order::~Order() = default;

Shipment::Shipment() = default;
Shipment::Shipment(const Shipment& other) = default;
Shipment::Shipment(Shipment&& other) = default;
Shipment& Shipment::operator=(const Shipment& other) = default;
Shipment& Shipment::operator=(Shipment&& other) = default;
Shipment::~Shipment() = default;

DriversLicense::DriversLicense() = default;
DriversLicense::DriversLicense(const DriversLicense& other) = default;
DriversLicense::DriversLicense(DriversLicense&& other) = default;
DriversLicense& DriversLicense::operator=(const DriversLicense& other) =
    default;
DriversLicense& DriversLicense::operator=(DriversLicense&& other) = default;
DriversLicense::~DriversLicense() = default;

Passport::Passport() = default;
Passport::Passport(const Passport& other) = default;
Passport::Passport(Passport&& other) = default;
Passport& Passport::operator=(const Passport& other) = default;
Passport& Passport::operator=(Passport&& other) = default;
Passport::~Passport() = default;

NationalId::NationalId() = default;
NationalId::NationalId(const NationalId& other) = default;
NationalId::NationalId(NationalId&& other) = default;
NationalId& NationalId::operator=(const NationalId& other) = default;
NationalId& NationalId::operator=(NationalId&& other) = default;
NationalId::~NationalId() = default;

Vehicle::Vehicle() = default;
Vehicle::Vehicle(const Vehicle& other) = default;
Vehicle::Vehicle(Vehicle&& other) = default;
Vehicle& Vehicle::operator=(const Vehicle& other) = default;
Vehicle& Vehicle::operator=(Vehicle&& other) = default;
Vehicle::~Vehicle() = default;

GmailSource::GmailSource() = default;
GmailSource::GmailSource(const GmailSource& other) = default;
GmailSource::GmailSource(GmailSource&& other) = default;
GmailSource& GmailSource::operator=(const GmailSource& other) = default;
GmailSource& GmailSource::operator=(GmailSource&& other) = default;
GmailSource::~GmailSource() = default;

CalendarSource::CalendarSource() = default;
CalendarSource::CalendarSource(const CalendarSource& other) = default;
CalendarSource::CalendarSource(CalendarSource&& other) = default;
CalendarSource& CalendarSource::operator=(const CalendarSource& other) =
    default;
CalendarSource& CalendarSource::operator=(CalendarSource&& other) = default;
CalendarSource::~CalendarSource() = default;

PhotosSource::PhotosSource() = default;
PhotosSource::PhotosSource(const PhotosSource& other) = default;
PhotosSource::PhotosSource(PhotosSource&& other) = default;
PhotosSource& PhotosSource::operator=(const PhotosSource& other) = default;
PhotosSource& PhotosSource::operator=(PhotosSource&& other) = default;
PhotosSource::~PhotosSource() = default;

Source::Source() = default;
Source::Source(const Source& other) = default;
Source::Source(Source&& other) = default;
Source& Source::operator=(const Source& other) = default;
Source& Source::operator=(Source&& other) = default;
Source::~Source() = default;

Entity::Entity() = default;
Entity::Entity(const Entity& other) = default;
Entity::Entity(Entity&& other) = default;
Entity& Entity::operator=(const Entity& other) = default;
Entity& Entity::operator=(Entity&& other) = default;
Entity::~Entity() = default;

EntityType Entity::GetType() const {
  return std::visit(
      absl::Overload(
          [](const FlightReservation&) {
            return EntityType::kFlightReservation;
          },
          [](const Order&) { return EntityType::kOrder; },
          [](const Shipment&) { return EntityType::kShipment; },
          [](const DriversLicense&) { return EntityType::kDriversLicense; },
          [](const Passport&) { return EntityType::kPassport; },
          [](const NationalId&) { return EntityType::kNationalId; },
          [](const Vehicle&) { return EntityType::kVehicle; }),
      specifics);
}

}  // namespace accessibility_annotator
