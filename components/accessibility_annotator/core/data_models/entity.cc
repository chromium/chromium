// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/data_models/entity.h"

#include <variant>

#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace accessibility_annotator {

Flight::Flight() = default;
Flight::~Flight() = default;
Flight::Flight(const Flight& other) = default;
Flight::Flight(Flight&& other) = default;
Flight& Flight::operator=(const Flight& other) = default;
Flight& Flight::operator=(Flight&& other) = default;

Order::ItemDescription::ItemDescription() = default;
Order::ItemDescription::~ItemDescription() = default;
Order::ItemDescription::ItemDescription(const ItemDescription& other) = default;
Order::ItemDescription::ItemDescription(ItemDescription&& other) = default;
Order::ItemDescription& Order::ItemDescription::operator=(
    const ItemDescription& other) = default;
Order::ItemDescription& Order::ItemDescription::operator=(
    ItemDescription&& other) = default;

Order::Order() = default;
Order::~Order() = default;
Order::Order(const Order& other) = default;
Order::Order(Order&& other) = default;
Order& Order::operator=(const Order& other) = default;
Order& Order::operator=(Order&& other) = default;

Shipment::Shipment() = default;
Shipment::~Shipment() = default;
Shipment::Shipment(const Shipment& other) = default;
Shipment::Shipment(Shipment&& other) = default;
Shipment& Shipment::operator=(const Shipment& other) = default;
Shipment& Shipment::operator=(Shipment&& other) = default;

DriverLicense::DriverLicense() = default;
DriverLicense::~DriverLicense() = default;
DriverLicense::DriverLicense(const DriverLicense& other) = default;
DriverLicense::DriverLicense(DriverLicense&& other) = default;
DriverLicense& DriverLicense::operator=(const DriverLicense& other) = default;
DriverLicense& DriverLicense::operator=(DriverLicense&& other) = default;

Passport::Passport() = default;
Passport::~Passport() = default;
Passport::Passport(const Passport& other) = default;
Passport::Passport(Passport&& other) = default;
Passport& Passport::operator=(const Passport& other) = default;
Passport& Passport::operator=(Passport&& other) = default;

NationalId::NationalId() = default;
NationalId::~NationalId() = default;
NationalId::NationalId(const NationalId& other) = default;
NationalId::NationalId(NationalId&& other) = default;
NationalId& NationalId::operator=(const NationalId& other) = default;
NationalId& NationalId::operator=(NationalId&& other) = default;

Entity::Entity() = default;
Entity::~Entity() = default;
Entity::Entity(const Entity& other) = default;
Entity::Entity(Entity&& other) = default;
Entity& Entity::operator=(const Entity& other) = default;
Entity& Entity::operator=(Entity&& other) = default;

EntityType Entity::GetType() const {
  return std::visit(
      absl::Overload(
          [](const Flight&) { return EntityType::kFlight; },
          [](const Order&) { return EntityType::kOrder; },
          [](const Shipment&) { return EntityType::kShipment; },
          [](const DriverLicense&) { return EntityType::kDriverLicense; },
          [](const Passport&) { return EntityType::kPassport; },
          [](const NationalId&) { return EntityType::kNationalId; }),
      specifics);
}

}  // namespace accessibility_annotator
