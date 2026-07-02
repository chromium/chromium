// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/memory_data_type_util.h"

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
    base::flat_map<MemoryDataType, std::vector<AttributeValue>>;

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
                       MemoryDataType memory_data_type,
                       AttributeMap& map,
                       std::vector<AttributeValue>* full_attributes = nullptr) {
  if (!IsAttributeValueEmpty(value)) {
    map[memory_data_type] = {value};
    if (full_attributes) {
      full_attributes->push_back(value);
    }
  }
}

template <typename T>
void AddAttributeValue(const std::optional<T>& value,
                       MemoryDataType memory_data_type,
                       AttributeMap& map,
                       std::vector<AttributeValue>* full_attributes = nullptr) {
  if (value) {
    AddAttributeValue(*value, memory_data_type, map, full_attributes);
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
                                      MemoryDataType memory_data_type) {
  std::vector<AttributeValue> target_attributes;
  auto it = map.find(memory_data_type);
  if (it != map.end()) {
    target_attributes = std::move(it->second);
    map.erase(it);
  }
  return AttributeResult(std::move(target_attributes), std::move(map));
}

// Returns true if the entry type is a full entity type i.e. kVehicle,
// kPassportFull, etc. Does not include name and address types as they are not
// entity representations.
bool IsFullMemoryDataType(MemoryDataType memory_data_type) {
  switch (memory_data_type) {
    case MemoryDataType::kVehicle:
    case MemoryDataType::kPassportFull:
    case MemoryDataType::kFlightReservationFull:
    case MemoryDataType::kNationalIdCardFull:
    case MemoryDataType::kRedressNumberFull:
    case MemoryDataType::kKnownTravelerNumberFull:
    case MemoryDataType::kDriversLicenseFull:
    case MemoryDataType::kOrderFull:
    case MemoryDataType::kShipmentFull:
      return true;
    case MemoryDataType::kNameFull:
    case MemoryDataType::kAddressFull:
    case MemoryDataType::kAddressStreetAddress:
    case MemoryDataType::kAddressCity:
    case MemoryDataType::kAddressState:
    case MemoryDataType::kAddressZip:
    case MemoryDataType::kAddressCountry:
    case MemoryDataType::kPhone:
    case MemoryDataType::kEmail:
    case MemoryDataType::kCompanyName:
    case MemoryDataType::kIban:
    case MemoryDataType::kIbanNickname:
    case MemoryDataType::kVehicleMake:
    case MemoryDataType::kVehicleModel:
    case MemoryDataType::kVehicleYear:
    case MemoryDataType::kVehicleOwner:
    case MemoryDataType::kVehiclePlateNumber:
    case MemoryDataType::kVehiclePlateState:
    case MemoryDataType::kVehicleVin:
    case MemoryDataType::kPassportName:
    case MemoryDataType::kPassportCountry:
    case MemoryDataType::kPassportNumber:
    case MemoryDataType::kPassportIssueDate:
    case MemoryDataType::kPassportExpirationDate:
    case MemoryDataType::kFlightReservationFlightNumber:
    case MemoryDataType::kFlightReservationTicketNumber:
    case MemoryDataType::kFlightReservationConfirmationCode:
    case MemoryDataType::kFlightReservationPassengerName:
    case MemoryDataType::kFlightReservationDepartureAirport:
    case MemoryDataType::kFlightReservationArrivalAirport:
    case MemoryDataType::kFlightReservationDepartureDate:
    case MemoryDataType::kFlightReservationArrivalDate:
    case MemoryDataType::kShipmentTrackingNumber:
    case MemoryDataType::kShipmentAssociatedOrderId:
    case MemoryDataType::kShipmentDeliveryAddress:
    case MemoryDataType::kShipmentDeliveryZipCode:
    case MemoryDataType::kShipmentCarrierName:
    case MemoryDataType::kShipmentCarrierDomain:
    case MemoryDataType::kShipmentEstimatedDeliveryDate:
    case MemoryDataType::kShipmentShippedDate:
    case MemoryDataType::kNationalIdCardName:
    case MemoryDataType::kNationalIdCardCountry:
    case MemoryDataType::kNationalIdCardNumber:
    case MemoryDataType::kNationalIdCardIssueDate:
    case MemoryDataType::kNationalIdCardExpirationDate:
    case MemoryDataType::kRedressNumberName:
    case MemoryDataType::kRedressNumberNumber:
    case MemoryDataType::kKnownTravelerNumberName:
    case MemoryDataType::kKnownTravelerNumberNumber:
    case MemoryDataType::kKnownTravelerNumberExpirationDate:
    case MemoryDataType::kDriversLicenseName:
    case MemoryDataType::kDriversLicenseState:
    case MemoryDataType::kDriversLicenseNumber:
    case MemoryDataType::kDriversLicenseIssueDate:
    case MemoryDataType::kDriversLicenseExpirationDate:
    case MemoryDataType::kOrderId:
    case MemoryDataType::kOrderAccount:
    case MemoryDataType::kOrderDate:
    case MemoryDataType::kOrderMerchantName:
    case MemoryDataType::kOrderMerchantDomain:
    case MemoryDataType::kOrderProductNames:
    case MemoryDataType::kOrderGrandTotal:
    case MemoryDataType::kCreditCardNumber:
    case MemoryDataType::kCreditCardExpirationDate:
    case MemoryDataType::kCreditCardSecurityCode:
    case MemoryDataType::kCreditCardNameOnCard:
    case MemoryDataType::kCreditCardNickname:
    case MemoryDataType::kUnknown:
      return false;
  }
  NOTREACHED();
}

MemorySearchResult CreateMemorySearchResultFromAttributeResult(
    MemoryDataType memory_data_type,
    const AttributeResult& result) {
  std::u16string target_value;
  if (!result.target_attributes.empty()) {
    std::vector<std::u16string> target_strings =
        base::ToVector(result.target_attributes, SerializeAttributeValue);
    target_value = base::JoinString(target_strings, u", ");
  }

  // TODO(crbug.com/493849593): Update confidence score.
  MemorySearchResult memory_search_result(
      memory_data_type, /*type_name=*/std::u16string(), target_value,
      /*confidence_score=*/0.0);
  for (const auto& [other_entry_type, other_attribute_values] :
       result.other_attributes) {
    if (other_attribute_values.empty() ||
        IsFullMemoryDataType(other_entry_type)) {
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
                                          MemoryDataType memory_data_type) {
  AttributeMap map;
  std::vector<AttributeValue> vehicle_full;

  AddAttributeValue(vehicle.make, MemoryDataType::kVehicleMake, map,
                    &vehicle_full);
  AddAttributeValue(vehicle.model, MemoryDataType::kVehicleModel, map,
                    &vehicle_full);
  AddAttributeValue(vehicle.year, MemoryDataType::kVehicleYear, map,
                    &vehicle_full);
  AddAttributeValue(vehicle.owner, MemoryDataType::kVehicleOwner, map,
                    &vehicle_full);
  AddAttributeValue(vehicle.plate_number, MemoryDataType::kVehiclePlateNumber,
                    map, &vehicle_full);
  AddAttributeValue(vehicle.plate_state, MemoryDataType::kVehiclePlateState,
                    map, &vehicle_full);
  AddAttributeValue(vehicle.vin, MemoryDataType::kVehicleVin, map,
                    &vehicle_full);

  AddAttributeValue(vehicle.owner, MemoryDataType::kNameFull, map);

  if (!vehicle_full.empty()) {
    map[MemoryDataType::kVehicle] = std::move(vehicle_full);
  }

  return CreateAttributeResult(std::move(map), memory_data_type);
}

AttributeResult GetPassportAttributeResult(const Passport& passport,
                                           MemoryDataType memory_data_type) {
  AttributeMap map;
  std::vector<AttributeValue> passport_full;

  AddAttributeValue(passport.name, MemoryDataType::kPassportName, map,
                    &passport_full);
  AddAttributeValue(passport.issuing_country, MemoryDataType::kPassportCountry,
                    map, &passport_full);
  AddAttributeValue(passport.number, MemoryDataType::kPassportNumber, map,
                    &passport_full);

  AddAttributeValue(passport.name, MemoryDataType::kNameFull, map);

  AddAttributeValue(passport.issue_date, MemoryDataType::kPassportIssueDate,
                    map, &passport_full);
  AddAttributeValue(passport.expiration_date,
                    MemoryDataType::kPassportExpirationDate, map,
                    &passport_full);

  if (!passport_full.empty()) {
    map[MemoryDataType::kPassportFull] = std::move(passport_full);
  }

  return CreateAttributeResult(std::move(map), memory_data_type);
}

AttributeResult GetFlightReservationAttributeResult(
    const FlightReservation& flight,
    MemoryDataType memory_data_type) {
  AttributeMap map;
  std::vector<AttributeValue> flight_full;

  AddAttributeValue(flight.flight_number,
                    MemoryDataType::kFlightReservationFlightNumber, map,
                    &flight_full);
  AddAttributeValue(flight.ticket_number,
                    MemoryDataType::kFlightReservationTicketNumber, map,
                    &flight_full);
  AddAttributeValue(flight.confirmation_code,
                    MemoryDataType::kFlightReservationConfirmationCode, map,
                    &flight_full);
  AddAttributeValue(flight.passenger_name,
                    MemoryDataType::kFlightReservationPassengerName, map,
                    &flight_full);
  AddAttributeValue(flight.departure_airport,
                    MemoryDataType::kFlightReservationDepartureAirport, map,
                    &flight_full);
  AddAttributeValue(flight.arrival_airport,
                    MemoryDataType::kFlightReservationArrivalAirport, map,
                    &flight_full);

  AddAttributeValue(flight.passenger_name, MemoryDataType::kNameFull, map);

  AddAttributeValue(flight.departure_date,
                    MemoryDataType::kFlightReservationDepartureDate, map,
                    &flight_full);
  AddAttributeValue(flight.arrival_date,
                    MemoryDataType::kFlightReservationArrivalDate, map,
                    &flight_full);

  if (!flight_full.empty()) {
    map[MemoryDataType::kFlightReservationFull] = std::move(flight_full);
  }

  return CreateAttributeResult(std::move(map), memory_data_type);
}

AttributeResult GetNationalIdAttributeResult(const NationalId& national_id,
                                             MemoryDataType memory_data_type) {
  AttributeMap map;
  std::vector<AttributeValue> national_id_full;

  AddAttributeValue(national_id.name, MemoryDataType::kNationalIdCardName, map,
                    &national_id_full);
  AddAttributeValue(national_id.issuing_country,
                    MemoryDataType::kNationalIdCardCountry, map,
                    &national_id_full);
  AddAttributeValue(national_id.number, MemoryDataType::kNationalIdCardNumber,
                    map, &national_id_full);

  AddAttributeValue(national_id.name, MemoryDataType::kNameFull, map);

  AddAttributeValue(national_id.issue_date,
                    MemoryDataType::kNationalIdCardIssueDate, map,
                    &national_id_full);
  AddAttributeValue(national_id.expiration_date,
                    MemoryDataType::kNationalIdCardExpirationDate, map,
                    &national_id_full);

  if (!national_id_full.empty()) {
    map[MemoryDataType::kNationalIdCardFull] = std::move(national_id_full);
  }

  return CreateAttributeResult(std::move(map), memory_data_type);
}

AttributeResult GetDriversLicenseAttributeResult(
    const DriversLicense& dl,
    MemoryDataType memory_data_type) {
  AttributeMap map;
  std::vector<AttributeValue> dl_full;

  AddAttributeValue(dl.name, MemoryDataType::kDriversLicenseName, map,
                    &dl_full);
  AddAttributeValue(dl.state, MemoryDataType::kDriversLicenseState, map,
                    &dl_full);
  AddAttributeValue(dl.number, MemoryDataType::kDriversLicenseNumber, map,
                    &dl_full);

  AddAttributeValue(dl.name, MemoryDataType::kNameFull, map);

  AddAttributeValue(dl.issue_date, MemoryDataType::kDriversLicenseIssueDate,
                    map, &dl_full);
  AddAttributeValue(dl.expiration_date,
                    MemoryDataType::kDriversLicenseExpirationDate, map,
                    &dl_full);

  if (!dl_full.empty()) {
    map[MemoryDataType::kDriversLicenseFull] = std::move(dl_full);
  }

  return CreateAttributeResult(std::move(map), memory_data_type);
}

AttributeResult GetOrderAttributeResult(const Order& order,
                                        MemoryDataType memory_data_type) {
  AttributeMap map;
  std::vector<AttributeValue> order_full;

  AddAttributeValue(order.id, MemoryDataType::kOrderId, map, &order_full);
  AddAttributeValue(order.account, MemoryDataType::kOrderAccount, map,
                    &order_full);
  AddAttributeValue(order.merchant_name, MemoryDataType::kOrderMerchantName,
                    map, &order_full);
  AddAttributeValue(order.merchant_domain, MemoryDataType::kOrderMerchantDomain,
                    map, &order_full);
  AddAttributeValue(order.products, MemoryDataType::kOrderProductNames, map,
                    &order_full);
  AddAttributeValue(order.grand_total, MemoryDataType::kOrderGrandTotal, map,
                    &order_full);

  AddAttributeValue(order.order_date, MemoryDataType::kOrderDate, map,
                    &order_full);

  if (!order_full.empty()) {
    map[MemoryDataType::kOrderFull] = std::move(order_full);
  }

  return CreateAttributeResult(std::move(map), memory_data_type);
}

AttributeResult GetShipmentAttributeResult(const Shipment& shipment,
                                           MemoryDataType memory_data_type) {
  AttributeMap map;
  std::vector<AttributeValue> shipment_full;

  AddAttributeValue(shipment.tracking_number,
                    MemoryDataType::kShipmentTrackingNumber, map,
                    &shipment_full);
  AddAttributeValue(shipment.associated_order_id,
                    MemoryDataType::kShipmentAssociatedOrderId, map,
                    &shipment_full);
  AddAttributeValue(shipment.delivery_address,
                    MemoryDataType::kShipmentDeliveryAddress, map,
                    &shipment_full);
  AddAttributeValue(shipment.carrier_name, MemoryDataType::kShipmentCarrierName,
                    map, &shipment_full);
  AddAttributeValue(shipment.carrier_domain,
                    MemoryDataType::kShipmentCarrierDomain, map,
                    &shipment_full);

  AddAttributeValue(shipment.shipped_date, MemoryDataType::kShipmentShippedDate,
                    map, &shipment_full);

  if (!shipment_full.empty()) {
    map[MemoryDataType::kShipmentFull] = std::move(shipment_full);
  }

  return CreateAttributeResult(std::move(map), memory_data_type);
}

}  // namespace

MemorySearchResult CreateResultFromEntity(MemoryDataType memory_data_type,
                                          const Entity& entity) {
  return std::visit(
      absl::Overload{
          [&](const Vehicle& vehicle) {
            return CreateMemorySearchResultFromAttributeResult(
                memory_data_type,
                GetVehicleAttributeResult(vehicle, memory_data_type));
          },
          [&](const Passport& passport) {
            return CreateMemorySearchResultFromAttributeResult(
                memory_data_type,
                GetPassportAttributeResult(passport, memory_data_type));
          },
          [&](const FlightReservation& flight) {
            return CreateMemorySearchResultFromAttributeResult(
                memory_data_type,
                GetFlightReservationAttributeResult(flight, memory_data_type));
          },
          [&](const NationalId& national_id) {
            return CreateMemorySearchResultFromAttributeResult(
                memory_data_type,
                GetNationalIdAttributeResult(national_id, memory_data_type));
          },
          [&](const DriversLicense& dl) {
            return CreateMemorySearchResultFromAttributeResult(
                memory_data_type,
                GetDriversLicenseAttributeResult(dl, memory_data_type));
          },
          [&](const Order& order) {
            return CreateMemorySearchResultFromAttributeResult(
                memory_data_type,
                GetOrderAttributeResult(order, memory_data_type));
          },
          [&](const Shipment& shipment) {
            return CreateMemorySearchResultFromAttributeResult(
                memory_data_type,
                GetShipmentAttributeResult(shipment, memory_data_type));
          }},
      entity.specifics);
}

EntityTypeEnumSet GetEntityTypesForMemoryDataType(
    MemoryDataType memory_data_type) {
  switch (memory_data_type) {
    case MemoryDataType::kVehicle:
    case MemoryDataType::kVehicleMake:
    case MemoryDataType::kVehicleModel:
    case MemoryDataType::kVehicleYear:
    case MemoryDataType::kVehicleOwner:
    case MemoryDataType::kVehiclePlateNumber:
    case MemoryDataType::kVehiclePlateState:
    case MemoryDataType::kVehicleVin:
      return {EntityType::kVehicle};
    case MemoryDataType::kPassportFull:
    case MemoryDataType::kPassportName:
    case MemoryDataType::kPassportCountry:
    case MemoryDataType::kPassportNumber:
    case MemoryDataType::kPassportIssueDate:
    case MemoryDataType::kPassportExpirationDate:
      return {EntityType::kPassport};
    case MemoryDataType::kFlightReservationFull:
    case MemoryDataType::kFlightReservationFlightNumber:
    case MemoryDataType::kFlightReservationTicketNumber:
    case MemoryDataType::kFlightReservationConfirmationCode:
    case MemoryDataType::kFlightReservationPassengerName:
    case MemoryDataType::kFlightReservationDepartureAirport:
    case MemoryDataType::kFlightReservationArrivalAirport:
    case MemoryDataType::kFlightReservationDepartureDate:
    case MemoryDataType::kFlightReservationArrivalDate:
      return {EntityType::kFlightReservation};
    case MemoryDataType::kShipmentFull:
    case MemoryDataType::kShipmentTrackingNumber:
    case MemoryDataType::kShipmentAssociatedOrderId:
    case MemoryDataType::kShipmentDeliveryAddress:
    case MemoryDataType::kShipmentDeliveryZipCode:
    case MemoryDataType::kShipmentCarrierName:
    case MemoryDataType::kShipmentCarrierDomain:
    case MemoryDataType::kShipmentEstimatedDeliveryDate:
    case MemoryDataType::kShipmentShippedDate:
      return {EntityType::kShipment};
    case MemoryDataType::kNationalIdCardFull:
    case MemoryDataType::kNationalIdCardName:
    case MemoryDataType::kNationalIdCardCountry:
    case MemoryDataType::kNationalIdCardNumber:
    case MemoryDataType::kNationalIdCardIssueDate:
    case MemoryDataType::kNationalIdCardExpirationDate:
      return {EntityType::kNationalId};
    case MemoryDataType::kDriversLicenseFull:
    case MemoryDataType::kDriversLicenseName:
    case MemoryDataType::kDriversLicenseState:
    case MemoryDataType::kDriversLicenseNumber:
    case MemoryDataType::kDriversLicenseIssueDate:
    case MemoryDataType::kDriversLicenseExpirationDate:
      return {EntityType::kDriversLicense};
    case MemoryDataType::kOrderFull:
    case MemoryDataType::kOrderId:
    case MemoryDataType::kOrderAccount:
    case MemoryDataType::kOrderDate:
    case MemoryDataType::kOrderMerchantName:
    case MemoryDataType::kOrderMerchantDomain:
    case MemoryDataType::kOrderProductNames:
    case MemoryDataType::kOrderGrandTotal:
      return {EntityType::kOrder};
    case MemoryDataType::kNameFull:
      return {EntityType::kVehicle,           EntityType::kPassport,
              EntityType::kFlightReservation, EntityType::kNationalId,
              EntityType::kDriversLicense,    EntityType::kOrder};
    case MemoryDataType::kUnknown:
    case MemoryDataType::kAddressFull:
    case MemoryDataType::kAddressStreetAddress:
    case MemoryDataType::kAddressCity:
    case MemoryDataType::kAddressState:
    case MemoryDataType::kAddressZip:
    case MemoryDataType::kAddressCountry:
    case MemoryDataType::kPhone:
    case MemoryDataType::kEmail:
    case MemoryDataType::kCompanyName:
    case MemoryDataType::kIban:
    case MemoryDataType::kIbanNickname:
    case MemoryDataType::kCreditCardNumber:
    case MemoryDataType::kCreditCardExpirationDate:
    case MemoryDataType::kCreditCardSecurityCode:
    case MemoryDataType::kCreditCardNameOnCard:
    case MemoryDataType::kCreditCardNickname:
    case MemoryDataType::kRedressNumberFull:
    case MemoryDataType::kRedressNumberName:
    case MemoryDataType::kRedressNumberNumber:
    case MemoryDataType::kKnownTravelerNumberFull:
    case MemoryDataType::kKnownTravelerNumberName:
    case MemoryDataType::kKnownTravelerNumberNumber:
    case MemoryDataType::kKnownTravelerNumberExpirationDate:
      return {EntityType::kUnknown};
  }
  NOTREACHED();
}

bool IsSpiiMemoryDataType(MemoryDataType type) {
  switch (type) {
    case MemoryDataType::kIban:
    case MemoryDataType::kCreditCardNumber:
    case MemoryDataType::kCreditCardSecurityCode:
    case MemoryDataType::kPassportNumber:
    case MemoryDataType::kPassportFull:
    case MemoryDataType::kNationalIdCardNumber:
    case MemoryDataType::kNationalIdCardFull:
    case MemoryDataType::kDriversLicenseNumber:
    case MemoryDataType::kDriversLicenseFull:
    case MemoryDataType::kRedressNumberNumber:
    case MemoryDataType::kRedressNumberFull:
    case MemoryDataType::kKnownTravelerNumberFull:
    case MemoryDataType::kKnownTravelerNumberNumber:
      return true;
    case MemoryDataType::kNameFull:
    case MemoryDataType::kAddressFull:
    case MemoryDataType::kAddressStreetAddress:
    case MemoryDataType::kAddressCity:
    case MemoryDataType::kAddressState:
    case MemoryDataType::kAddressZip:
    case MemoryDataType::kAddressCountry:
    case MemoryDataType::kPhone:
    case MemoryDataType::kCompanyName:
    case MemoryDataType::kEmail:
    case MemoryDataType::kIbanNickname:
    case MemoryDataType::kVehicle:
    case MemoryDataType::kVehicleMake:
    case MemoryDataType::kVehicleModel:
    case MemoryDataType::kVehicleYear:
    case MemoryDataType::kVehicleOwner:
    case MemoryDataType::kVehiclePlateNumber:
    case MemoryDataType::kVehiclePlateState:
    case MemoryDataType::kVehicleVin:
    case MemoryDataType::kPassportName:
    case MemoryDataType::kPassportCountry:
    case MemoryDataType::kPassportIssueDate:
    case MemoryDataType::kPassportExpirationDate:
    case MemoryDataType::kFlightReservationFull:
    case MemoryDataType::kFlightReservationFlightNumber:
    case MemoryDataType::kFlightReservationTicketNumber:
    case MemoryDataType::kFlightReservationConfirmationCode:
    case MemoryDataType::kFlightReservationPassengerName:
    case MemoryDataType::kFlightReservationDepartureAirport:
    case MemoryDataType::kFlightReservationArrivalAirport:
    case MemoryDataType::kFlightReservationDepartureDate:
    case MemoryDataType::kFlightReservationArrivalDate:
    case MemoryDataType::kNationalIdCardName:
    case MemoryDataType::kNationalIdCardCountry:
    case MemoryDataType::kNationalIdCardIssueDate:
    case MemoryDataType::kNationalIdCardExpirationDate:
    case MemoryDataType::kDriversLicenseName:
    case MemoryDataType::kDriversLicenseState:
    case MemoryDataType::kDriversLicenseIssueDate:
    case MemoryDataType::kDriversLicenseExpirationDate:
    case MemoryDataType::kRedressNumberName:
    case MemoryDataType::kKnownTravelerNumberName:
    case MemoryDataType::kKnownTravelerNumberExpirationDate:
    case MemoryDataType::kCreditCardExpirationDate:
    case MemoryDataType::kCreditCardNameOnCard:
    case MemoryDataType::kCreditCardNickname:
    case MemoryDataType::kOrderFull:
    case MemoryDataType::kOrderId:
    case MemoryDataType::kOrderAccount:
    case MemoryDataType::kOrderDate:
    case MemoryDataType::kShipmentShippedDate:
    case MemoryDataType::kOrderMerchantName:
    case MemoryDataType::kOrderMerchantDomain:
    case MemoryDataType::kOrderProductNames:
    case MemoryDataType::kOrderGrandTotal:
    case MemoryDataType::kShipmentFull:
    case MemoryDataType::kShipmentTrackingNumber:
    case MemoryDataType::kShipmentAssociatedOrderId:
    case MemoryDataType::kShipmentDeliveryAddress:
    case MemoryDataType::kShipmentDeliveryZipCode:
    case MemoryDataType::kShipmentCarrierName:
    case MemoryDataType::kShipmentCarrierDomain:
    case MemoryDataType::kShipmentEstimatedDeliveryDate:
    case MemoryDataType::kUnknown:
      return false;
  }
  NOTREACHED();
}

}  // namespace accessibility_annotator
