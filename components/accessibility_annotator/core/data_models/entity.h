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

// LINT.IfChange(AttributeDefinitions)

struct FlightReservation {
  FlightReservation();
  FlightReservation(const FlightReservation& other);
  FlightReservation(FlightReservation&& other);
  FlightReservation& operator=(const FlightReservation& other);
  FlightReservation& operator=(FlightReservation&& other);
  ~FlightReservation();

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
    ItemDescription(const ItemDescription& other);
    ItemDescription(ItemDescription&& other);
    ItemDescription& operator=(const ItemDescription& other);
    ItemDescription& operator=(ItemDescription&& other);
    ~ItemDescription();

    std::string name;
    int quantity = 0;
    std::string description;
  };

  Order();
  Order(const Order& other);
  Order(Order&& other);
  Order& operator=(const Order& other);
  Order& operator=(Order&& other);
  ~Order();

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
  Shipment(const Shipment& other);
  Shipment(Shipment&& other);
  Shipment& operator=(const Shipment& other);
  Shipment& operator=(Shipment&& other);
  ~Shipment();

  std::string tracking_number;
  std::string associated_order_id;
  std::string delivery_address;
  std::string carrier_name;
  GURL carrier_domain;
  std::optional<Date> estimated_delivery_date;
};

struct DriversLicense {
  DriversLicense();
  DriversLicense(const DriversLicense& other);
  DriversLicense(DriversLicense&& other);
  DriversLicense& operator=(const DriversLicense& other);
  DriversLicense& operator=(DriversLicense&& other);
  ~DriversLicense();

  std::string name;
  std::string number;
  std::optional<Date> expiration_date;
  std::optional<Date> issue_date;
  std::string state;
};

struct Passport {
  Passport();
  Passport(const Passport& other);
  Passport(Passport&& other);
  Passport& operator=(const Passport& other);
  Passport& operator=(Passport&& other);
  ~Passport();

  std::string name;
  std::string number;
  std::optional<Date> expiration_date;
  std::optional<Date> issue_date;
  std::string issuing_country;
};

struct NationalId {
  NationalId();
  NationalId(const NationalId& other);
  NationalId(NationalId&& other);
  NationalId& operator=(const NationalId& other);
  NationalId& operator=(NationalId&& other);
  ~NationalId();

  std::string name;
  std::string number;
  std::optional<Date> expiration_date;
  std::optional<Date> issue_date;
  std::string issuing_country;
};

struct Vehicle {
  Vehicle();
  Vehicle(const Vehicle& other);
  Vehicle(Vehicle&& other);
  Vehicle& operator=(const Vehicle& other);
  Vehicle& operator=(Vehicle&& other);
  ~Vehicle();

  std::string make;
  std::string model;
  std::string year;
  std::string owner;
  std::string plate_number;
  std::string plate_state;
  std::string vin;
};

// LINT.ThenChange(//core/browser/data_model/autofill_ai/from_accessibility_annotator.cc:AttributeConversions)

struct GmailSource {
  GmailSource();
  GmailSource(const GmailSource& other);
  GmailSource(GmailSource&& other);
  GmailSource& operator=(const GmailSource& other);
  GmailSource& operator=(GmailSource&& other);
  ~GmailSource();

  std::string thread_id;
  std::string message_id;
  std::string thread_locator;
  base::Time received_time;
};

struct CalendarSource {
  CalendarSource();
  CalendarSource(const CalendarSource& other);
  CalendarSource(CalendarSource&& other);
  CalendarSource& operator=(const CalendarSource& other);
  CalendarSource& operator=(CalendarSource&& other);
  ~CalendarSource();

  std::string event_id;
  base::Time modified_time;
};

struct PhotosSource {
  PhotosSource();
  PhotosSource(const PhotosSource& other);
  PhotosSource(PhotosSource&& other);
  PhotosSource& operator=(const PhotosSource& other);
  PhotosSource& operator=(PhotosSource&& other);
  ~PhotosSource();

  std::string photo_id;
  base::Time creation_time;
};

// The source of an entity. This is a single source that is referenced by the
// entity. For example, a Gmail message can be a source for an Order entity.
struct Source {
  using SourceSpecifics =
      std::variant<GmailSource, CalendarSource, PhotosSource>;

  Source();
  Source(const Source& other);
  Source(Source&& other);
  Source& operator=(const Source& other);
  Source& operator=(Source&& other);
  ~Source();

  GURL deeplink;
  SourceSpecifics specifics;
};

struct Entity {
  using EntitySpecifics = std::variant<FlightReservation,
                                       Order,
                                       Shipment,
                                       DriversLicense,
                                       Passport,
                                       NationalId,
                                       Vehicle>;

  Entity();
  Entity(const Entity& other);
  Entity(Entity&& other);
  Entity& operator=(const Entity& other);
  Entity& operator=(Entity&& other);
  ~Entity();

  // Returns the type of the entity.
  EntityType GetType() const;

  std::string entity_id;
  std::vector<Source> sources;
  EntitySpecifics specifics;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_DATA_MODELS_ENTITY_H_
