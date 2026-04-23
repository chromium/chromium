// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/entry_type_util.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/to_vector.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace accessibility_annotator {

namespace {

using AttributeValue = std::variant<std::string,
                                    Date,
                                    base::Time,
                                    GURL,
                                    std::vector<Order::ItemDescription>>;

using AttributeMap = base::flat_map<EntryType, std::vector<AttributeValue>>;

struct AttributeResult {
  AttributeResult(std::vector<AttributeValue> target_attributes,
                  AttributeMap other_attributes)
      : target_attributes(std::move(target_attributes)),
        other_attributes(std::move(other_attributes)) {}

  std::vector<AttributeValue> target_attributes;
  AttributeMap other_attributes;
};

template <typename T>
bool IsAttributeValueEmpty(const T& value) {
  return false;
}

template <>
bool IsAttributeValueEmpty(const std::string& str) {
  return str.empty();
}

template <>
bool IsAttributeValueEmpty(const std::vector<Order::ItemDescription>& items) {
  return items.empty();
}

template <>
bool IsAttributeValueEmpty(const GURL& url) {
  return !url.is_valid();
}

template <>
bool IsAttributeValueEmpty(const base::Time& time) {
  return time.is_null();
}

template <typename T>
void AddAttributeValue(const T& value,
                       EntryType entry_type,
                       AttributeMap& map,
                       std::vector<AttributeValue>* full_attributes = nullptr) {
  if (!IsAttributeValueEmpty(value)) {
    map[entry_type] = {value};
    if (full_attributes) {
      full_attributes->push_back(value);
    }
  }
}

template <typename T>
void AddAttributeValue(const std::optional<T>& value,
                       EntryType entry_type,
                       AttributeMap& map,
                       std::vector<AttributeValue>* full_attributes = nullptr) {
  if (value) {
    AddAttributeValue(*value, entry_type, map, full_attributes);
  }
}

std::u16string SerializeDate(int year, int month, int day) {
  return base::ASCIIToUTF16(
      base::StringPrintf("%04d-%02d-%02d", year, month, day));
}

// String representation of the attribute value.
// Date -> YYYY-MM-DD
// base::Time -> YYYY-MM-DD
// GURL -> spec()
// std::vector<Order::ItemDescription> -> comma separated list of item names
std::u16string SerializeAttributeValue(const AttributeValue& value) {
  return std::visit(
      absl::Overload{
          [](const std::string& str) { return base::UTF8ToUTF16(str); },
          [](const Date& date) {
            return SerializeDate(date.year, date.month, date.day);
          },
          [](const base::Time& time) {
            base::Time::Exploded exploded;
            time.UTCExplode(&exploded);
            return SerializeDate(exploded.year, exploded.month,
                                 exploded.day_of_month);
          },
          [](const GURL& url) {
            return base::UTF8ToUTF16(url.is_valid() ? url.spec()
                                                    : std::string());
          },
          [](const std::vector<Order::ItemDescription>& items) {
            std::vector<std::u16string> item_strings =
                base::ToVector(items, [](const Order::ItemDescription& item) {
                  return base::UTF8ToUTF16(item.name);
                });
            return base::JoinString(item_strings, u", ");
          }},
      value);
}

// Splits the attribute map into two parts: target attributes and other
// attributes. Target attributes are the attributes that match the intent type.
// Other attributes are the attributes that do not match the intent type.
AttributeResult CreateAttributeResult(AttributeMap map, EntryType entry_type) {
  std::vector<AttributeValue> target_attributes;
  auto it = map.find(entry_type);
  if (it != map.end()) {
    target_attributes = std::move(it->second);
    map.erase(it);
  }
  return AttributeResult(std::move(target_attributes), std::move(map));
}

// Returns true if the entry type is a full entity type i.e. kVehicle,
// kPassportFull, etc. Does not include name and address types as they are not
// entity representations.
bool IsFullEntryType(EntryType entry_type) {
  switch (entry_type) {
    case EntryType::kVehicle:
    case EntryType::kPassportFull:
    case EntryType::kFlightReservationFull:
    case EntryType::kNationalIdCardFull:
    case EntryType::kRedressNumberFull:
    case EntryType::kKnownTravelerNumberFull:
    case EntryType::kDriversLicenseFull:
    case EntryType::kOrderFull:
    case EntryType::kShipmentFull:
      return true;
    case EntryType::kNameFull:
    case EntryType::kAddressFull:
    case EntryType::kAddressStreetAddress:
    case EntryType::kAddressCity:
    case EntryType::kAddressState:
    case EntryType::kAddressZip:
    case EntryType::kAddressCountry:
    case EntryType::kPhone:
    case EntryType::kEmail:
    case EntryType::kCompanyName:
    case EntryType::kIban:
    case EntryType::kIbanNickname:
    case EntryType::kVehicleMake:
    case EntryType::kVehicleModel:
    case EntryType::kVehicleYear:
    case EntryType::kVehicleOwner:
    case EntryType::kVehiclePlateNumber:
    case EntryType::kVehiclePlateState:
    case EntryType::kVehicleVin:
    case EntryType::kPassportName:
    case EntryType::kPassportCountry:
    case EntryType::kPassportNumber:
    case EntryType::kPassportIssueDate:
    case EntryType::kPassportExpirationDate:
    case EntryType::kFlightReservationFlightNumber:
    case EntryType::kFlightReservationTicketNumber:
    case EntryType::kFlightReservationConfirmationCode:
    case EntryType::kFlightReservationPassengerName:
    case EntryType::kFlightReservationDepartureAirport:
    case EntryType::kFlightReservationArrivalAirport:
    case EntryType::kFlightReservationDepartureDate:
    case EntryType::kFlightReservationArrivalDate:
    case EntryType::kShipmentTrackingNumber:
    case EntryType::kShipmentAssociatedOrderId:
    case EntryType::kShipmentDeliveryAddress:
    case EntryType::kShipmentDeliveryZipCode:
    case EntryType::kShipmentCarrierName:
    case EntryType::kShipmentCarrierDomain:
    case EntryType::kShipmentEstimatedDeliveryDate:
    case EntryType::kNationalIdCardName:
    case EntryType::kNationalIdCardCountry:
    case EntryType::kNationalIdCardNumber:
    case EntryType::kNationalIdCardIssueDate:
    case EntryType::kNationalIdCardExpirationDate:
    case EntryType::kRedressNumberName:
    case EntryType::kRedressNumberNumber:
    case EntryType::kKnownTravelerNumberName:
    case EntryType::kKnownTravelerNumberNumber:
    case EntryType::kKnownTravelerNumberExpirationDate:
    case EntryType::kDriversLicenseName:
    case EntryType::kDriversLicenseState:
    case EntryType::kDriversLicenseNumber:
    case EntryType::kDriversLicenseIssueDate:
    case EntryType::kDriversLicenseExpirationDate:
    case EntryType::kOrderId:
    case EntryType::kOrderAccount:
    case EntryType::kOrderDate:
    case EntryType::kOrderMerchantName:
    case EntryType::kOrderMerchantDomain:
    case EntryType::kOrderProductNames:
    case EntryType::kOrderGrandTotal:
    case EntryType::kCreditCardNumber:
    case EntryType::kCreditCardExpirationDate:
    case EntryType::kCreditCardSecurityCode:
    case EntryType::kCreditCardNameOnCard:
    case EntryType::kCreditCardNickname:
    case EntryType::kUnknown:
      return false;
  }

  NOTREACHED();
}

MemorySearchResult CreateMemorySearchResultFromAttributeResult(
    EntryType entry_type,
    const AttributeResult& result) {
  std::u16string target_value;
  if (!result.target_attributes.empty()) {
    std::vector<std::u16string> target_strings =
        base::ToVector(result.target_attributes, SerializeAttributeValue);
    target_value = base::JoinString(target_strings, u", ");
  }

  // TODO(crbug.com/493849593): Update confidence score.
  MemorySearchResult memory_search_result(
      entry_type, /*type_name=*/std::u16string(), target_value,
      /*confidence_score=*/0.0);
  for (const auto& [other_entry_type, other_attribute_values] :
       result.other_attributes) {
    if (other_attribute_values.empty() || IsFullEntryType(other_entry_type)) {
      continue;
    }
    // TODO(crbug.com/493849593) Localize type name.
    memory_search_result.metadata_list.emplace_back(
        /*type=*/other_entry_type,
        /*type_name=*/std::u16string(),
        /*value=*/SerializeAttributeValue(other_attribute_values.front()));
  }
  // TODO(crbug.com/493849593): Update source to include kCalendar.
  memory_search_result.sources = {
      MemoryEntrySource(MemoryEntrySourceType::kGmail)};
  return memory_search_result;
}

AttributeResult GetVehicleAttributeResult(const Vehicle& vehicle,
                                          EntryType entry_type) {
  AttributeMap map;
  std::vector<AttributeValue> vehicle_full;

  AddAttributeValue(vehicle.make, EntryType::kVehicleMake, map, &vehicle_full);
  AddAttributeValue(vehicle.model, EntryType::kVehicleModel, map,
                    &vehicle_full);
  AddAttributeValue(vehicle.year, EntryType::kVehicleYear, map, &vehicle_full);
  AddAttributeValue(vehicle.owner, EntryType::kVehicleOwner, map,
                    &vehicle_full);
  AddAttributeValue(vehicle.plate_number, EntryType::kVehiclePlateNumber, map,
                    &vehicle_full);
  AddAttributeValue(vehicle.plate_state, EntryType::kVehiclePlateState, map,
                    &vehicle_full);
  AddAttributeValue(vehicle.vin, EntryType::kVehicleVin, map, &vehicle_full);

  AddAttributeValue(vehicle.owner, EntryType::kNameFull, map);

  if (!vehicle_full.empty()) {
    map[EntryType::kVehicle] = std::move(vehicle_full);
  }

  return CreateAttributeResult(std::move(map), entry_type);
}

AttributeResult GetPassportAttributeResult(const Passport& passport,
                                           EntryType entry_type) {
  AttributeMap map;
  std::vector<AttributeValue> passport_full;

  AddAttributeValue(passport.name, EntryType::kPassportName, map,
                    &passport_full);
  AddAttributeValue(passport.issuing_country, EntryType::kPassportCountry, map,
                    &passport_full);
  AddAttributeValue(passport.number, EntryType::kPassportNumber, map,
                    &passport_full);

  AddAttributeValue(passport.name, EntryType::kNameFull, map);

  AddAttributeValue(passport.issue_date, EntryType::kPassportIssueDate, map,
                    &passport_full);
  AddAttributeValue(passport.expiration_date,
                    EntryType::kPassportExpirationDate, map, &passport_full);

  if (!passport_full.empty()) {
    map[EntryType::kPassportFull] = std::move(passport_full);
  }

  return CreateAttributeResult(std::move(map), entry_type);
}

AttributeResult GetFlightReservationAttributeResult(
    const FlightReservation& flight,
    EntryType entry_type) {
  AttributeMap map;
  std::vector<AttributeValue> flight_full;

  AddAttributeValue(flight.flight_number,
                    EntryType::kFlightReservationFlightNumber, map,
                    &flight_full);
  AddAttributeValue(flight.ticket_number,
                    EntryType::kFlightReservationTicketNumber, map,
                    &flight_full);
  AddAttributeValue(flight.confirmation_code,
                    EntryType::kFlightReservationConfirmationCode, map,
                    &flight_full);
  AddAttributeValue(flight.passenger_name,
                    EntryType::kFlightReservationPassengerName, map,
                    &flight_full);
  AddAttributeValue(flight.departure_airport,
                    EntryType::kFlightReservationDepartureAirport, map,
                    &flight_full);
  AddAttributeValue(flight.arrival_airport,
                    EntryType::kFlightReservationArrivalAirport, map,
                    &flight_full);

  AddAttributeValue(flight.passenger_name, EntryType::kNameFull, map);

  AddAttributeValue(flight.departure_date,
                    EntryType::kFlightReservationDepartureDate, map,
                    &flight_full);
  AddAttributeValue(flight.arrival_date,
                    EntryType::kFlightReservationArrivalDate, map,
                    &flight_full);

  if (!flight_full.empty()) {
    map[EntryType::kFlightReservationFull] = std::move(flight_full);
  }

  return CreateAttributeResult(std::move(map), entry_type);
}

AttributeResult GetNationalIdAttributeResult(const NationalId& national_id,
                                             EntryType entry_type) {
  AttributeMap map;
  std::vector<AttributeValue> national_id_full;

  AddAttributeValue(national_id.name, EntryType::kNationalIdCardName, map,
                    &national_id_full);
  AddAttributeValue(national_id.issuing_country,
                    EntryType::kNationalIdCardCountry, map, &national_id_full);
  AddAttributeValue(national_id.number, EntryType::kNationalIdCardNumber, map,
                    &national_id_full);

  AddAttributeValue(national_id.name, EntryType::kNameFull, map);

  AddAttributeValue(national_id.issue_date, EntryType::kNationalIdCardIssueDate,
                    map, &national_id_full);
  AddAttributeValue(national_id.expiration_date,
                    EntryType::kNationalIdCardExpirationDate, map,
                    &national_id_full);

  if (!national_id_full.empty()) {
    map[EntryType::kNationalIdCardFull] = std::move(national_id_full);
  }

  return CreateAttributeResult(std::move(map), entry_type);
}

AttributeResult GetDriversLicenseAttributeResult(const DriversLicense& dl,
                                                 EntryType entry_type) {
  AttributeMap map;
  std::vector<AttributeValue> dl_full;

  AddAttributeValue(dl.name, EntryType::kDriversLicenseName, map, &dl_full);
  AddAttributeValue(dl.state, EntryType::kDriversLicenseState, map, &dl_full);
  AddAttributeValue(dl.number, EntryType::kDriversLicenseNumber, map, &dl_full);

  AddAttributeValue(dl.name, EntryType::kNameFull, map);

  AddAttributeValue(dl.issue_date, EntryType::kDriversLicenseIssueDate, map,
                    &dl_full);
  AddAttributeValue(dl.expiration_date,
                    EntryType::kDriversLicenseExpirationDate, map, &dl_full);

  if (!dl_full.empty()) {
    map[EntryType::kDriversLicenseFull] = std::move(dl_full);
  }

  return CreateAttributeResult(std::move(map), entry_type);
}

AttributeResult GetOrderAttributeResult(const Order& order,
                                        EntryType entry_type) {
  AttributeMap map;
  std::vector<AttributeValue> order_full;

  AddAttributeValue(order.id, EntryType::kOrderId, map, &order_full);
  AddAttributeValue(order.account, EntryType::kOrderAccount, map, &order_full);
  AddAttributeValue(order.merchant_name, EntryType::kOrderMerchantName, map,
                    &order_full);
  AddAttributeValue(order.merchant_domain, EntryType::kOrderMerchantDomain, map,
                    &order_full);
  AddAttributeValue(order.products, EntryType::kOrderProductNames, map,
                    &order_full);
  AddAttributeValue(order.grand_total, EntryType::kOrderGrandTotal, map,
                    &order_full);

  AddAttributeValue(order.order_date, EntryType::kOrderDate, map, &order_full);

  if (!order_full.empty()) {
    map[EntryType::kOrderFull] = std::move(order_full);
  }

  return CreateAttributeResult(std::move(map), entry_type);
}

AttributeResult GetShipmentAttributeResult(const Shipment& shipment,
                                           EntryType entry_type) {
  AttributeMap map;
  std::vector<AttributeValue> shipment_full;

  AddAttributeValue(shipment.tracking_number,
                    EntryType::kShipmentTrackingNumber, map, &shipment_full);
  AddAttributeValue(shipment.associated_order_id,
                    EntryType::kShipmentAssociatedOrderId, map, &shipment_full);
  AddAttributeValue(shipment.delivery_address,
                    EntryType::kShipmentDeliveryAddress, map, &shipment_full);
  AddAttributeValue(shipment.carrier_name, EntryType::kShipmentCarrierName, map,
                    &shipment_full);
  AddAttributeValue(shipment.carrier_domain, EntryType::kShipmentCarrierDomain,
                    map, &shipment_full);

  AddAttributeValue(shipment.estimated_delivery_date,
                    EntryType::kShipmentEstimatedDeliveryDate, map,
                    &shipment_full);

  if (!shipment_full.empty()) {
    map[EntryType::kShipmentFull] = std::move(shipment_full);
  }

  return CreateAttributeResult(std::move(map), entry_type);
}

}  // namespace

MemorySearchResult CreateResultFromEntity(EntryType entry_type,
                                          const Entity& entity) {
  return std::visit(
      absl::Overload{
          [&](const Vehicle& vehicle) {
            return CreateMemorySearchResultFromAttributeResult(
                entry_type, GetVehicleAttributeResult(vehicle, entry_type));
          },
          [&](const Passport& passport) {
            return CreateMemorySearchResultFromAttributeResult(
                entry_type, GetPassportAttributeResult(passport, entry_type));
          },
          [&](const FlightReservation& flight) {
            return CreateMemorySearchResultFromAttributeResult(
                entry_type,
                GetFlightReservationAttributeResult(flight, entry_type));
          },
          [&](const NationalId& national_id) {
            return CreateMemorySearchResultFromAttributeResult(
                entry_type,
                GetNationalIdAttributeResult(national_id, entry_type));
          },
          [&](const DriversLicense& dl) {
            return CreateMemorySearchResultFromAttributeResult(
                entry_type, GetDriversLicenseAttributeResult(dl, entry_type));
          },
          [&](const Order& order) {
            return CreateMemorySearchResultFromAttributeResult(
                entry_type, GetOrderAttributeResult(order, entry_type));
          },
          [&](const Shipment& shipment) {
            return CreateMemorySearchResultFromAttributeResult(
                entry_type, GetShipmentAttributeResult(shipment, entry_type));
          }},
      entity.specifics);
}

EntityTypeEnumSet GetEntityTypesForEntryType(EntryType entry_type) {
  switch (entry_type) {
    case EntryType::kVehicle:
    case EntryType::kVehicleMake:
    case EntryType::kVehicleModel:
    case EntryType::kVehicleYear:
    case EntryType::kVehicleOwner:
    case EntryType::kVehiclePlateNumber:
    case EntryType::kVehiclePlateState:
    case EntryType::kVehicleVin:
      return {EntityType::kVehicle};
    case EntryType::kPassportFull:
    case EntryType::kPassportName:
    case EntryType::kPassportCountry:
    case EntryType::kPassportNumber:
    case EntryType::kPassportIssueDate:
    case EntryType::kPassportExpirationDate:
      return {EntityType::kPassport};
    case EntryType::kFlightReservationFull:
    case EntryType::kFlightReservationFlightNumber:
    case EntryType::kFlightReservationTicketNumber:
    case EntryType::kFlightReservationConfirmationCode:
    case EntryType::kFlightReservationPassengerName:
    case EntryType::kFlightReservationDepartureAirport:
    case EntryType::kFlightReservationArrivalAirport:
    case EntryType::kFlightReservationDepartureDate:
    case EntryType::kFlightReservationArrivalDate:
      return {EntityType::kFlightReservation};
    case EntryType::kShipmentFull:
    case EntryType::kShipmentTrackingNumber:
    case EntryType::kShipmentAssociatedOrderId:
    case EntryType::kShipmentDeliveryAddress:
    case EntryType::kShipmentDeliveryZipCode:
    case EntryType::kShipmentCarrierName:
    case EntryType::kShipmentCarrierDomain:
    case EntryType::kShipmentEstimatedDeliveryDate:
      return {EntityType::kShipment};
    case EntryType::kNationalIdCardFull:
    case EntryType::kNationalIdCardName:
    case EntryType::kNationalIdCardCountry:
    case EntryType::kNationalIdCardNumber:
    case EntryType::kNationalIdCardIssueDate:
    case EntryType::kNationalIdCardExpirationDate:
      return {EntityType::kNationalId};
    case EntryType::kDriversLicenseFull:
    case EntryType::kDriversLicenseName:
    case EntryType::kDriversLicenseState:
    case EntryType::kDriversLicenseNumber:
    case EntryType::kDriversLicenseIssueDate:
    case EntryType::kDriversLicenseExpirationDate:
      return {EntityType::kDriversLicense};
    case EntryType::kOrderFull:
    case EntryType::kOrderId:
    case EntryType::kOrderAccount:
    case EntryType::kOrderDate:
    case EntryType::kOrderMerchantName:
    case EntryType::kOrderMerchantDomain:
    case EntryType::kOrderProductNames:
    case EntryType::kOrderGrandTotal:
      return {EntityType::kOrder};
    case EntryType::kNameFull:
      return {EntityType::kVehicle,           EntityType::kPassport,
              EntityType::kFlightReservation, EntityType::kNationalId,
              EntityType::kDriversLicense,    EntityType::kOrder};
    case EntryType::kUnknown:
    case EntryType::kAddressFull:
    case EntryType::kAddressStreetAddress:
    case EntryType::kAddressCity:
    case EntryType::kAddressState:
    case EntryType::kAddressZip:
    case EntryType::kAddressCountry:
    case EntryType::kPhone:
    case EntryType::kEmail:
    case EntryType::kCompanyName:
    case EntryType::kIban:
    case EntryType::kIbanNickname:
    case EntryType::kCreditCardNumber:
    case EntryType::kCreditCardExpirationDate:
    case EntryType::kCreditCardSecurityCode:
    case EntryType::kCreditCardNameOnCard:
    case EntryType::kCreditCardNickname:
    case EntryType::kRedressNumberFull:
    case EntryType::kRedressNumberName:
    case EntryType::kRedressNumberNumber:
    case EntryType::kKnownTravelerNumberFull:
    case EntryType::kKnownTravelerNumberName:
    case EntryType::kKnownTravelerNumberNumber:
    case EntryType::kKnownTravelerNumberExpirationDate:
      return {EntityType::kUnknown};
  }

  NOTREACHED();
}

}  // namespace accessibility_annotator
