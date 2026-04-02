// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type_util.h"

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

using AttributeMap =
    base::flat_map<QueryIntentType, std::vector<AttributeValue>>;

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
                       QueryIntentType intent_type,
                       AttributeMap& map,
                       std::vector<AttributeValue>* full_attributes = nullptr) {
  if (!IsAttributeValueEmpty(value)) {
    map[intent_type] = {value};
    if (full_attributes) {
      full_attributes->push_back(value);
    }
  }
}

template <typename T>
void AddAttributeValue(const std::optional<T>& value,
                       QueryIntentType intent_type,
                       AttributeMap& map,
                       std::vector<AttributeValue>* full_attributes = nullptr) {
  if (value) {
    AddAttributeValue(*value, intent_type, map, full_attributes);
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
AttributeResult CreateAttributeResult(AttributeMap map,
                                      QueryIntentType intent_type) {
  std::vector<AttributeValue> target_attributes;
  auto it = map.find(intent_type);
  if (it != map.end()) {
    target_attributes = std::move(it->second);
    map.erase(it);
  }
  return AttributeResult(std::move(target_attributes), std::move(map));
}

// Returns true if the query intent type is a full entity type i.e. kVehicle,
// kPassportFull, etc. Does not include name and address types as they are not
// entity representations.
bool IsFullQueryIntentType(QueryIntentType intent_type) {
  switch (intent_type) {
    case QueryIntentType::kVehicle:
    case QueryIntentType::kPassportFull:
    case QueryIntentType::kFlightReservationFull:
    case QueryIntentType::kNationalIdCardFull:
    case QueryIntentType::kRedressNumberFull:
    case QueryIntentType::kKnownTravelerNumberFull:
    case QueryIntentType::kDriversLicenseFull:
    case QueryIntentType::kOrderFull:
    case QueryIntentType::kShipmentFull:
    case QueryIntentType::kCreditCardFull:
      return true;
    case QueryIntentType::kNameFull:
    case QueryIntentType::kAddressFull:
    case QueryIntentType::kAddressStreetAddress:
    case QueryIntentType::kAddressCity:
    case QueryIntentType::kAddressState:
    case QueryIntentType::kAddressZip:
    case QueryIntentType::kAddressCountry:
    case QueryIntentType::kPhone:
    case QueryIntentType::kEmail:
    case QueryIntentType::kCompanyName:
    case QueryIntentType::kIban:
    case QueryIntentType::kIbanNickname:
    case QueryIntentType::kVehicleMake:
    case QueryIntentType::kVehicleModel:
    case QueryIntentType::kVehicleYear:
    case QueryIntentType::kVehicleOwner:
    case QueryIntentType::kVehiclePlateNumber:
    case QueryIntentType::kVehiclePlateState:
    case QueryIntentType::kVehicleVin:
    case QueryIntentType::kPassportName:
    case QueryIntentType::kPassportCountry:
    case QueryIntentType::kPassportNumber:
    case QueryIntentType::kPassportIssueDate:
    case QueryIntentType::kPassportExpirationDate:
    case QueryIntentType::kFlightReservationFlightNumber:
    case QueryIntentType::kFlightReservationTicketNumber:
    case QueryIntentType::kFlightReservationConfirmationCode:
    case QueryIntentType::kFlightReservationPassengerName:
    case QueryIntentType::kFlightReservationDepartureAirport:
    case QueryIntentType::kFlightReservationArrivalAirport:
    case QueryIntentType::kFlightReservationDepartureDate:
    case QueryIntentType::kFlightReservationArrivalDate:
    case QueryIntentType::kShipmentTrackingNumber:
    case QueryIntentType::kShipmentAssociatedOrderId:
    case QueryIntentType::kShipmentDeliveryAddress:
    case QueryIntentType::kShipmentCarrierName:
    case QueryIntentType::kShipmentCarrierDomain:
    case QueryIntentType::kShipmentEstimatedDeliveryDate:
    case QueryIntentType::kNationalIdCardName:
    case QueryIntentType::kNationalIdCardCountry:
    case QueryIntentType::kNationalIdCardNumber:
    case QueryIntentType::kNationalIdCardIssueDate:
    case QueryIntentType::kNationalIdCardExpirationDate:
    case QueryIntentType::kRedressNumberName:
    case QueryIntentType::kRedressNumberNumber:
    case QueryIntentType::kKnownTravelerNumberName:
    case QueryIntentType::kKnownTravelerNumberNumber:
    case QueryIntentType::kKnownTravelerNumberExpirationDate:
    case QueryIntentType::kDriversLicenseName:
    case QueryIntentType::kDriversLicenseState:
    case QueryIntentType::kDriversLicenseNumber:
    case QueryIntentType::kDriversLicenseIssueDate:
    case QueryIntentType::kDriversLicenseExpirationDate:
    case QueryIntentType::kOrderId:
    case QueryIntentType::kOrderAccount:
    case QueryIntentType::kOrderDate:
    case QueryIntentType::kOrderMerchantName:
    case QueryIntentType::kOrderMerchantDomain:
    case QueryIntentType::kOrderProductNames:
    case QueryIntentType::kOrderGrandTotal:
    case QueryIntentType::kCreditCardNumber:
    case QueryIntentType::kCreditCardExpirationDate:
    case QueryIntentType::kCreditCardSecurityCode:
    case QueryIntentType::kCreditCardNameOnCard:
    case QueryIntentType::kCreditCardNickname:
    case QueryIntentType::kUnknown:
      return false;
  }

  NOTREACHED();
}

MemorySearchResult CreateMemorySearchResultFromAttributeResult(
    QueryIntentType intent_type,
    const AttributeResult& result) {
  std::u16string target_value;
  if (!result.target_attributes.empty()) {
    std::vector<std::u16string> target_strings =
        base::ToVector(result.target_attributes, SerializeAttributeValue);
    target_value = base::JoinString(target_strings, u", ");
  }

  // TODO(crbug.com/493849593): Update confidence score.
  MemorySearchResult memory_search_result(
      intent_type, /*type_name=*/std::u16string(), target_value,
      /*confidence_score=*/0.0);
  for (const auto& [other_intent_type, other_attribute_values] :
       result.other_attributes) {
    if (other_attribute_values.empty() ||
        IsFullQueryIntentType(other_intent_type)) {
      continue;
    }
    // TODO(crbug.com/493849593) Localize type name.
    memory_search_result.metadata_list.emplace_back(
        /*type=*/other_intent_type,
        /*type_name=*/std::u16string(),
        /*value=*/SerializeAttributeValue(other_attribute_values.front()));
  }
  // TODO(crbug.com/493849593): Update source to include kCalendar.
  memory_search_result.sources = {
      MemoryEntrySource(MemoryEntrySourceType::kGmail)};
  return memory_search_result;
}

AttributeResult GetVehicleAttributeResult(const Vehicle& vehicle,
                                          QueryIntentType intent_type) {
  AttributeMap map;
  std::vector<AttributeValue> vehicle_full;

  AddAttributeValue(vehicle.make, QueryIntentType::kVehicleMake, map,
                    &vehicle_full);
  AddAttributeValue(vehicle.model, QueryIntentType::kVehicleModel, map,
                    &vehicle_full);
  AddAttributeValue(vehicle.year, QueryIntentType::kVehicleYear, map,
                    &vehicle_full);
  AddAttributeValue(vehicle.owner, QueryIntentType::kVehicleOwner, map,
                    &vehicle_full);
  AddAttributeValue(vehicle.plate_number, QueryIntentType::kVehiclePlateNumber,
                    map, &vehicle_full);
  AddAttributeValue(vehicle.plate_state, QueryIntentType::kVehiclePlateState,
                    map, &vehicle_full);
  AddAttributeValue(vehicle.vin, QueryIntentType::kVehicleVin, map,
                    &vehicle_full);

  AddAttributeValue(vehicle.owner, QueryIntentType::kNameFull, map);

  if (!vehicle_full.empty()) {
    map[QueryIntentType::kVehicle] = std::move(vehicle_full);
  }

  return CreateAttributeResult(std::move(map), intent_type);
}

AttributeResult GetPassportAttributeResult(const Passport& passport,
                                           QueryIntentType intent_type) {
  AttributeMap map;
  std::vector<AttributeValue> passport_full;

  AddAttributeValue(passport.name, QueryIntentType::kPassportName, map,
                    &passport_full);
  AddAttributeValue(passport.issuing_country, QueryIntentType::kPassportCountry,
                    map, &passport_full);
  AddAttributeValue(passport.number, QueryIntentType::kPassportNumber, map,
                    &passport_full);

  AddAttributeValue(passport.name, QueryIntentType::kNameFull, map);

  AddAttributeValue(passport.issue_date, QueryIntentType::kPassportIssueDate,
                    map, &passport_full);
  AddAttributeValue(passport.expiration_date,
                    QueryIntentType::kPassportExpirationDate, map,
                    &passport_full);

  if (!passport_full.empty()) {
    map[QueryIntentType::kPassportFull] = std::move(passport_full);
  }

  return CreateAttributeResult(std::move(map), intent_type);
}

AttributeResult GetFlightReservationAttributeResult(
    const FlightReservation& flight,
    QueryIntentType intent_type) {
  AttributeMap map;
  std::vector<AttributeValue> flight_full;

  AddAttributeValue(flight.flight_number,
                    QueryIntentType::kFlightReservationFlightNumber, map,
                    &flight_full);
  AddAttributeValue(flight.ticket_number,
                    QueryIntentType::kFlightReservationTicketNumber, map,
                    &flight_full);
  AddAttributeValue(flight.confirmation_code,
                    QueryIntentType::kFlightReservationConfirmationCode, map,
                    &flight_full);
  AddAttributeValue(flight.passenger_name,
                    QueryIntentType::kFlightReservationPassengerName, map,
                    &flight_full);
  AddAttributeValue(flight.departure_airport,
                    QueryIntentType::kFlightReservationDepartureAirport, map,
                    &flight_full);
  AddAttributeValue(flight.arrival_airport,
                    QueryIntentType::kFlightReservationArrivalAirport, map,
                    &flight_full);

  AddAttributeValue(flight.passenger_name, QueryIntentType::kNameFull, map);

  AddAttributeValue(flight.departure_date,
                    QueryIntentType::kFlightReservationDepartureDate, map,
                    &flight_full);
  AddAttributeValue(flight.arrival_date,
                    QueryIntentType::kFlightReservationArrivalDate, map,
                    &flight_full);

  if (!flight_full.empty()) {
    map[QueryIntentType::kFlightReservationFull] = std::move(flight_full);
  }

  return CreateAttributeResult(std::move(map), intent_type);
}

AttributeResult GetNationalIdAttributeResult(const NationalId& national_id,
                                             QueryIntentType intent_type) {
  AttributeMap map;
  std::vector<AttributeValue> national_id_full;

  AddAttributeValue(national_id.name, QueryIntentType::kNationalIdCardName, map,
                    &national_id_full);
  AddAttributeValue(national_id.issuing_country,
                    QueryIntentType::kNationalIdCardCountry, map,
                    &national_id_full);
  AddAttributeValue(national_id.number, QueryIntentType::kNationalIdCardNumber,
                    map, &national_id_full);

  AddAttributeValue(national_id.name, QueryIntentType::kNameFull, map);

  AddAttributeValue(national_id.issue_date,
                    QueryIntentType::kNationalIdCardIssueDate, map,
                    &national_id_full);
  AddAttributeValue(national_id.expiration_date,
                    QueryIntentType::kNationalIdCardExpirationDate, map,
                    &national_id_full);

  if (!national_id_full.empty()) {
    map[QueryIntentType::kNationalIdCardFull] = std::move(national_id_full);
  }

  return CreateAttributeResult(std::move(map), intent_type);
}

AttributeResult GetDriversLicenseAttributeResult(const DriversLicense& dl,
                                                 QueryIntentType intent_type) {
  AttributeMap map;
  std::vector<AttributeValue> dl_full;

  AddAttributeValue(dl.name, QueryIntentType::kDriversLicenseName, map,
                    &dl_full);
  AddAttributeValue(dl.state, QueryIntentType::kDriversLicenseState, map,
                    &dl_full);
  AddAttributeValue(dl.number, QueryIntentType::kDriversLicenseNumber, map,
                    &dl_full);

  AddAttributeValue(dl.name, QueryIntentType::kNameFull, map);

  AddAttributeValue(dl.issue_date, QueryIntentType::kDriversLicenseIssueDate,
                    map, &dl_full);
  AddAttributeValue(dl.expiration_date,
                    QueryIntentType::kDriversLicenseExpirationDate, map,
                    &dl_full);

  if (!dl_full.empty()) {
    map[QueryIntentType::kDriversLicenseFull] = std::move(dl_full);
  }

  return CreateAttributeResult(std::move(map), intent_type);
}

AttributeResult GetOrderAttributeResult(const Order& order,
                                        QueryIntentType intent_type) {
  AttributeMap map;
  std::vector<AttributeValue> order_full;

  AddAttributeValue(order.id, QueryIntentType::kOrderId, map, &order_full);
  AddAttributeValue(order.account, QueryIntentType::kOrderAccount, map,
                    &order_full);
  AddAttributeValue(order.merchant_name, QueryIntentType::kOrderMerchantName,
                    map, &order_full);
  AddAttributeValue(order.merchant_domain,
                    QueryIntentType::kOrderMerchantDomain, map, &order_full);
  AddAttributeValue(order.products, QueryIntentType::kOrderProductNames, map,
                    &order_full);
  AddAttributeValue(order.grand_total, QueryIntentType::kOrderGrandTotal, map,
                    &order_full);

  AddAttributeValue(order.order_date, QueryIntentType::kOrderDate, map,
                    &order_full);

  if (!order_full.empty()) {
    map[QueryIntentType::kOrderFull] = std::move(order_full);
  }

  return CreateAttributeResult(std::move(map), intent_type);
}

AttributeResult GetShipmentAttributeResult(const Shipment& shipment,
                                           QueryIntentType intent_type) {
  AttributeMap map;
  std::vector<AttributeValue> shipment_full;

  AddAttributeValue(shipment.tracking_number,
                    QueryIntentType::kShipmentTrackingNumber, map,
                    &shipment_full);
  AddAttributeValue(shipment.associated_order_id,
                    QueryIntentType::kShipmentAssociatedOrderId, map,
                    &shipment_full);
  AddAttributeValue(shipment.delivery_address,
                    QueryIntentType::kShipmentDeliveryAddress, map,
                    &shipment_full);
  AddAttributeValue(shipment.carrier_name,
                    QueryIntentType::kShipmentCarrierName, map, &shipment_full);
  AddAttributeValue(shipment.carrier_domain,
                    QueryIntentType::kShipmentCarrierDomain, map,
                    &shipment_full);

  AddAttributeValue(shipment.estimated_delivery_date,
                    QueryIntentType::kShipmentEstimatedDeliveryDate, map,
                    &shipment_full);

  if (!shipment_full.empty()) {
    map[QueryIntentType::kShipmentFull] = std::move(shipment_full);
  }

  return CreateAttributeResult(std::move(map), intent_type);
}

}  // namespace

MemorySearchResult CreateResultFromEntity(QueryIntentType intent_type,
                                          const Entity& entity) {
  return std::visit(
      absl::Overload{
          [&](const Vehicle& vehicle) {
            return CreateMemorySearchResultFromAttributeResult(
                intent_type, GetVehicleAttributeResult(vehicle, intent_type));
          },
          [&](const Passport& passport) {
            return CreateMemorySearchResultFromAttributeResult(
                intent_type, GetPassportAttributeResult(passport, intent_type));
          },
          [&](const FlightReservation& flight) {
            return CreateMemorySearchResultFromAttributeResult(
                intent_type,
                GetFlightReservationAttributeResult(flight, intent_type));
          },
          [&](const NationalId& national_id) {
            return CreateMemorySearchResultFromAttributeResult(
                intent_type,
                GetNationalIdAttributeResult(national_id, intent_type));
          },
          [&](const DriversLicense& dl) {
            return CreateMemorySearchResultFromAttributeResult(
                intent_type, GetDriversLicenseAttributeResult(dl, intent_type));
          },
          [&](const Order& order) {
            return CreateMemorySearchResultFromAttributeResult(
                intent_type, GetOrderAttributeResult(order, intent_type));
          },
          [&](const Shipment& shipment) {
            return CreateMemorySearchResultFromAttributeResult(
                intent_type, GetShipmentAttributeResult(shipment, intent_type));
          }},
      entity.specifics);
}

EntityTypeEnumSet GetEntityTypesForQueryIntentType(
    QueryIntentType intent_type) {
  switch (intent_type) {
    case QueryIntentType::kVehicle:
    case QueryIntentType::kVehicleMake:
    case QueryIntentType::kVehicleModel:
    case QueryIntentType::kVehicleYear:
    case QueryIntentType::kVehicleOwner:
    case QueryIntentType::kVehiclePlateNumber:
    case QueryIntentType::kVehiclePlateState:
    case QueryIntentType::kVehicleVin:
      return {EntityType::kVehicle};
    case QueryIntentType::kPassportFull:
    case QueryIntentType::kPassportName:
    case QueryIntentType::kPassportCountry:
    case QueryIntentType::kPassportNumber:
    case QueryIntentType::kPassportIssueDate:
    case QueryIntentType::kPassportExpirationDate:
      return {EntityType::kPassport};
    case QueryIntentType::kFlightReservationFull:
    case QueryIntentType::kFlightReservationFlightNumber:
    case QueryIntentType::kFlightReservationTicketNumber:
    case QueryIntentType::kFlightReservationConfirmationCode:
    case QueryIntentType::kFlightReservationPassengerName:
    case QueryIntentType::kFlightReservationDepartureAirport:
    case QueryIntentType::kFlightReservationArrivalAirport:
    case QueryIntentType::kFlightReservationDepartureDate:
    case QueryIntentType::kFlightReservationArrivalDate:
      return {EntityType::kFlightReservation};
    case QueryIntentType::kShipmentFull:
    case QueryIntentType::kShipmentTrackingNumber:
    case QueryIntentType::kShipmentAssociatedOrderId:
    case QueryIntentType::kShipmentDeliveryAddress:
    case QueryIntentType::kShipmentCarrierName:
    case QueryIntentType::kShipmentCarrierDomain:
    case QueryIntentType::kShipmentEstimatedDeliveryDate:
      return {EntityType::kShipment};
    case QueryIntentType::kNationalIdCardFull:
    case QueryIntentType::kNationalIdCardName:
    case QueryIntentType::kNationalIdCardCountry:
    case QueryIntentType::kNationalIdCardNumber:
    case QueryIntentType::kNationalIdCardIssueDate:
    case QueryIntentType::kNationalIdCardExpirationDate:
      return {EntityType::kNationalId};
    case QueryIntentType::kDriversLicenseFull:
    case QueryIntentType::kDriversLicenseName:
    case QueryIntentType::kDriversLicenseState:
    case QueryIntentType::kDriversLicenseNumber:
    case QueryIntentType::kDriversLicenseIssueDate:
    case QueryIntentType::kDriversLicenseExpirationDate:
      return {EntityType::kDriversLicense};
    case QueryIntentType::kOrderFull:
    case QueryIntentType::kOrderId:
    case QueryIntentType::kOrderAccount:
    case QueryIntentType::kOrderDate:
    case QueryIntentType::kOrderMerchantName:
    case QueryIntentType::kOrderMerchantDomain:
    case QueryIntentType::kOrderProductNames:
    case QueryIntentType::kOrderGrandTotal:
      return {EntityType::kOrder};
    case QueryIntentType::kNameFull:
      return {EntityType::kVehicle,           EntityType::kPassport,
              EntityType::kFlightReservation, EntityType::kNationalId,
              EntityType::kDriversLicense,    EntityType::kOrder};
    case QueryIntentType::kUnknown:
    case QueryIntentType::kAddressFull:
    case QueryIntentType::kAddressStreetAddress:
    case QueryIntentType::kAddressCity:
    case QueryIntentType::kAddressState:
    case QueryIntentType::kAddressZip:
    case QueryIntentType::kAddressCountry:
    case QueryIntentType::kPhone:
    case QueryIntentType::kEmail:
    case QueryIntentType::kCompanyName:
    case QueryIntentType::kIban:
    case QueryIntentType::kIbanNickname:
    case QueryIntentType::kCreditCardFull:
    case QueryIntentType::kCreditCardNumber:
    case QueryIntentType::kCreditCardExpirationDate:
    case QueryIntentType::kCreditCardSecurityCode:
    case QueryIntentType::kCreditCardNameOnCard:
    case QueryIntentType::kCreditCardNickname:
    case QueryIntentType::kRedressNumberFull:
    case QueryIntentType::kRedressNumberName:
    case QueryIntentType::kRedressNumberNumber:
    case QueryIntentType::kKnownTravelerNumberFull:
    case QueryIntentType::kKnownTravelerNumberName:
    case QueryIntentType::kKnownTravelerNumberNumber:
    case QueryIntentType::kKnownTravelerNumberExpirationDate:
      return {EntityType::kUnknown};
  }

  NOTREACHED();
}

}  // namespace accessibility_annotator
