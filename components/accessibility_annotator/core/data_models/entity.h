// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_DATA_MODELS_ENTITY_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_DATA_MODELS_ENTITY_H_

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/time/time.h"
#include "components/accessibility_annotator/core/data_models/entity_types.h"
#include "url/gurl.h"

namespace accessibility_annotator {

struct Date {
  friend bool operator==(const Date&, const Date&) = default;

  int day = 0;
  int month = 0;
  int year = 0;
};

struct Flight {
  Flight();
  ~Flight();
  Flight(const Flight& other);
  Flight(Flight&& other);
  Flight& operator=(const Flight& other);
  Flight& operator=(Flight&& other);

  std::string flight_number;
  std::string ticket_number;
  std::string confirmation_code;
  std::string passenger_name;
  std::string departure_airport;
  std::string arrival_airport;
  base::Time departure_date;
  base::Time arrival_date;
};

struct Order {
  struct ItemDescription {
    ItemDescription();
    ~ItemDescription();
    ItemDescription(const ItemDescription& other);
    ItemDescription(ItemDescription&& other);
    ItemDescription& operator=(const ItemDescription& other);
    ItemDescription& operator=(ItemDescription&& other);

    std::string name;
    int quantity = 0;
    std::string description;
  };

  Order();
  ~Order();
  Order(const Order& other);
  Order(Order&& other);
  Order& operator=(const Order& other);
  Order& operator=(Order&& other);

  std::string id;
  std::string account;
  std::optional<Date> order_date;
  std::string merchant_name;
  GURL merchant_domain;
  std::vector<ItemDescription> products;
  std::string grand_total;
};

struct Shipment {
  Shipment();
  ~Shipment();
  Shipment(const Shipment& other);
  Shipment(Shipment&& other);
  Shipment& operator=(const Shipment& other);
  Shipment& operator=(Shipment&& other);

  std::string tracking_number;
  std::string associated_order_id;
  std::string delivery_address;
  std::string carrier_name;
  GURL carrier_domain;
  std::optional<Date> estimated_delivery_date;
};

struct DriverLicense {
  DriverLicense();
  ~DriverLicense();
  DriverLicense(const DriverLicense& other);
  DriverLicense(DriverLicense&& other);
  DriverLicense& operator=(const DriverLicense& other);
  DriverLicense& operator=(DriverLicense&& other);

  std::string name;
  std::string number;
  std::optional<Date> expiration_date;
  std::optional<Date> issue_date;
  std::string state;
};

struct Passport {
  Passport();
  ~Passport();
  Passport(const Passport& other);
  Passport(Passport&& other);
  Passport& operator=(const Passport& other);
  Passport& operator=(Passport&& other);

  std::string name;
  std::string number;
  std::optional<Date> expiration_date;
  std::optional<Date> issue_date;
  std::string issuing_country;
};

struct NationalId {
  NationalId();
  ~NationalId();
  NationalId(const NationalId& other);
  NationalId(NationalId&& other);
  NationalId& operator=(const NationalId& other);
  NationalId& operator=(NationalId&& other);

  std::string name;
  std::string number;
  std::optional<Date> expiration_date;
  std::optional<Date> issue_date;
  std::string issuing_country;
};

struct Vehicle {
  Vehicle();
  ~Vehicle();
  Vehicle(const Vehicle& other);
  Vehicle(Vehicle&& other);
  Vehicle& operator=(const Vehicle& other);
  Vehicle& operator=(Vehicle&& other);

  std::string make;
  std::string model;
  std::string year;
  std::string owner;
  std::string plate_number;
  std::string plate_state;
  std::string vin;
};

struct Entity {
  using EntitySpecifics = std::variant<Flight,
                                       Order,
                                       Shipment,
                                       DriverLicense,
                                       Passport,
                                       NationalId,
                                       Vehicle>;

  Entity();
  ~Entity();
  Entity(const Entity& other);
  Entity(Entity&& other);
  Entity& operator=(const Entity& other);
  Entity& operator=(Entity&& other);

  // Returns the type of the entity.
  EntityType GetType() const;

  std::string entity_id;
  EntitySpecifics specifics;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_DATA_MODELS_ENTITY_H_
