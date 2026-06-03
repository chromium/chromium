// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/personal_context_conversion_util.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/i18n/time_formatting.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/personal_context/proto/features/common_data.pb.h"

namespace autofill {

namespace {

using enum AttributeTypeName;

std::u16string FormatDate(const personal_context::proto::Date& date) {
  if (date.year() <= 0 || date.month() <= 0 || date.day() <= 0) {
    return u"";
  }
  return base::UTF8ToUTF16(base::StringPrintf("%04d-%02d-%02d", date.year(),
                                              date.month(), date.day()));
}

std::u16string FormatTimestamp(
    const personal_context::proto::Timestamp& timestamp) {
  if (timestamp.seconds() == 0) {
    return u"";
  }
  base::Time time = base::Time::FromSecondsSinceUnixEpoch(timestamp.seconds());
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  return base::UTF8ToUTF16(base::StringPrintf(
      "%04d-%02d-%02d", exploded.year, exploded.month, exploded.day_of_month));
}

void AddAttribute(AttributeTypeName type,
                  const std::u16string& value,
                  std::vector<AttributeInstance>& attributes,
                  std::optional<AttributeInstance::MarkAsMaskedPasskey>
                      passkey = std::nullopt) {
  if (value.empty()) {
    return;
  }
  AttributeInstance attribute{AttributeType(type)};
  attribute.SetRawInfo(attribute.type().field_type(), value,
                       VerificationStatus::kNoStatus);
  if (passkey) {
    attribute.mark_as_masked(*passkey);
  }
  attribute.FinalizeInfo();
  attributes.push_back(std::move(attribute));
}

void AddStringAttribute(AttributeTypeName type,
                        std::string_view value,
                        std::vector<AttributeInstance>& attributes,
                        std::optional<AttributeInstance::MarkAsMaskedPasskey>
                            passkey = std::nullopt) {
  AddAttribute(type, base::UTF8ToUTF16(value), attributes, passkey);
}

EntityInstance CreateEntityInstance(EntityTypeName type_name,
                                    std::vector<AttributeInstance> attributes,
                                    std::string frecency_override = "") {
  return EntityInstance(
      EntityType(type_name),
      base::flat_set<AttributeInstance, AttributeInstance::CompareByType>(
          std::move(attributes)),
      EntityInstance::EntityId(
          base::Uuid::GenerateRandomV4().AsLowercaseString()),
      /*nickname=*/"",
      /*date_modified=*/base::Time::Now(),
      /*use_count=*/0,
      /*use_date=*/base::Time(), EntityInstance::RecordType::kPersonalContext,
      EntityInstance::AreAttributesReadOnly(true),
      std::move(frecency_override));
}

EntityInstance PersonalContextPassportToEntityInstance(
    const personal_context::proto::Passport& passport,
    std::optional<AttributeInstance::MarkAsMaskedPasskey> passkey) {
  std::vector<AttributeInstance> attributes;
  AddStringAttribute(kPassportName, passport.name(), attributes);
  AddStringAttribute(kPassportNumber, passport.number(), attributes, passkey);
  AddStringAttribute(kPassportCountry, passport.issuing_country(), attributes);
  if (passport.has_expiration_date()) {
    AddAttribute(kPassportExpirationDate,
                 FormatDate(passport.expiration_date()), attributes);
  }
  if (passport.has_issue_date()) {
    AddAttribute(kPassportIssueDate, FormatDate(passport.issue_date()),
                 attributes);
  }

  return CreateEntityInstance(EntityTypeName::kPassport, std::move(attributes));
}

EntityInstance PersonalContextDriversLicenseToEntityInstance(
    const personal_context::proto::DriversLicense& dl,
    std::optional<AttributeInstance::MarkAsMaskedPasskey> passkey) {
  std::vector<AttributeInstance> attributes;
  AddStringAttribute(kDriversLicenseName, dl.name(), attributes);
  AddStringAttribute(kDriversLicenseNumber, dl.number(), attributes, passkey);
  AddStringAttribute(kDriversLicenseState, dl.state(), attributes);
  if (dl.has_expiration_date()) {
    AddAttribute(kDriversLicenseExpirationDate,
                 FormatDate(dl.expiration_date()), attributes);
  }
  if (dl.has_issue_date()) {
    AddAttribute(kDriversLicenseIssueDate, FormatDate(dl.issue_date()),
                 attributes);
  }

  return CreateEntityInstance(EntityTypeName::kDriversLicense,
                              std::move(attributes));
}

EntityInstance PersonalContextNationalIdToEntityInstance(
    const personal_context::proto::NationalId& nid,
    std::optional<AttributeInstance::MarkAsMaskedPasskey> passkey) {
  std::vector<AttributeInstance> attributes;
  AddStringAttribute(kNationalIdCardName, nid.name(), attributes);
  AddStringAttribute(kNationalIdCardNumber, nid.number(), attributes, passkey);
  AddStringAttribute(kNationalIdCardCountry, nid.issuing_country(), attributes);
  if (nid.has_expiration_date()) {
    AddAttribute(kNationalIdCardExpirationDate,
                 FormatDate(nid.expiration_date()), attributes);
  }
  if (nid.has_issue_date()) {
    AddAttribute(kNationalIdCardIssueDate, FormatDate(nid.issue_date()),
                 attributes);
  }

  return CreateEntityInstance(EntityTypeName::kNationalIdCard,
                              std::move(attributes));
}

EntityInstance PersonalContextFlightReservationToEntityInstance(
    const personal_context::proto::FlightReservation& flight) {
  std::vector<AttributeInstance> attributes;
  AddStringAttribute(kFlightReservationFlightNumber, flight.flight_number(),
                     attributes);
  AddStringAttribute(kFlightReservationTicketNumber,
                     flight.flight_ticket_number(), attributes);
  AddStringAttribute(kFlightReservationConfirmationCode,
                     flight.flight_confirmation_code(), attributes);
  AddStringAttribute(kFlightReservationPassengerName, flight.passenger_name(),
                     attributes);
  AddStringAttribute(kFlightReservationDepartureAirport,
                     flight.departure_airport(), attributes);
  AddStringAttribute(kFlightReservationArrivalAirport, flight.arrival_airport(),
                     attributes);

  std::string frecency_override = "";
  if (flight.has_departure_time()) {
    AddAttribute(kFlightReservationDepartureDate,
                 FormatTimestamp(flight.departure_time()), attributes);
    base::Time departure_time = base::Time::FromSecondsSinceUnixEpoch(
        flight.departure_time().seconds());
    frecency_override = base::TimeFormatAsIso8601(departure_time);
  }

  return CreateEntityInstance(EntityTypeName::kFlightReservation,
                              std::move(attributes),
                              std::move(frecency_override));
}

EntityInstance PersonalContextVehicleToEntityInstance(
    const personal_context::proto::Vehicle& vehicle) {
  std::vector<AttributeInstance> attributes;
  AddStringAttribute(kVehicleMake, vehicle.vehicle_make(), attributes);
  AddStringAttribute(kVehicleModel, vehicle.vehicle_model(), attributes);
  AddStringAttribute(kVehicleYear, vehicle.vehicle_year(), attributes);
  AddStringAttribute(kVehicleVin, vehicle.vehicle_identification_number(),
                     attributes);
  AddStringAttribute(kVehiclePlateNumber, vehicle.vehicle_license_plate(),
                     attributes);
  AddStringAttribute(kVehiclePlateState, vehicle.license_plate_region(),
                     attributes);
  AddStringAttribute(kVehicleOwner, vehicle.owner_name(), attributes);

  return CreateEntityInstance(EntityTypeName::kVehicle, std::move(attributes));
}

EntityInstance PersonalContextOrderToEntityInstance(
    const personal_context::proto::Order& order) {
  std::vector<AttributeInstance> attributes;
  AddStringAttribute(kOrderId, order.order_id(), attributes);
  AddStringAttribute(kOrderAccount, order.account(), attributes);
  AddStringAttribute(kOrderMerchantName, order.merchant_name(), attributes);
  AddStringAttribute(kOrderMerchantDomain, order.merchant_domain(), attributes);
  if (order.has_order_time()) {
    AddAttribute(kOrderDate, FormatTimestamp(order.order_time()), attributes);
  }
  if (order.product_names_size() > 0) {
    std::vector<std::string> products(order.product_names().begin(),
                                      order.product_names().end());
    AddStringAttribute(kOrderProductNames, base::JoinString(products, ", "),
                       attributes);
  }

  return CreateEntityInstance(EntityTypeName::kOrder, std::move(attributes));
}

EntityInstance PersonalContextShipmentToEntityInstance(
    const personal_context::proto::Shipment& shipment) {
  std::vector<AttributeInstance> attributes;
  AddStringAttribute(kShipmentTrackingNumber, shipment.tracking_number(),
                     attributes);
  AddStringAttribute(kShipmentCarrierName, shipment.carrier_name(), attributes);
  AddStringAttribute(kShipmentCarrierDomain, shipment.carrier_domain(),
                     attributes);
  if (shipment.has_estimated_delivery_time()) {
    AddAttribute(kShipmentEstimatedDeliveryDate,
                 FormatTimestamp(shipment.estimated_delivery_time()),
                 attributes);
  }
  if (shipment.associated_order_ids_size() > 0) {
    std::vector<std::string> order_ids(shipment.associated_order_ids().begin(),
                                       shipment.associated_order_ids().end());
    AddStringAttribute(kShipmentOrderIds, base::JoinString(order_ids, ", "),
                       attributes);
  }

  return CreateEntityInstance(EntityTypeName::kShipment, std::move(attributes));
}

}  // namespace

std::optional<EntityInstance> PersonalContextEntityToEntityInstance(
    const personal_context::proto::Entity& entity,
    bool is_masked) {
  std::optional<AttributeInstance::MarkAsMaskedPasskey> passkey;
  if (is_masked) {
    passkey.emplace(AttributeInstance::MarkAsMaskedPasskey());
  }
  switch (entity.entity_case()) {
    case personal_context::proto::Entity::kPassport:
      return PersonalContextPassportToEntityInstance(entity.passport(),
                                                     passkey);
    case personal_context::proto::Entity::kDriversLicense:
      return PersonalContextDriversLicenseToEntityInstance(
          entity.drivers_license(), passkey);
    case personal_context::proto::Entity::kNationalId:
      return PersonalContextNationalIdToEntityInstance(entity.national_id(),
                                                       passkey);
    case personal_context::proto::Entity::kFlightReservation:
      return PersonalContextFlightReservationToEntityInstance(
          entity.flight_reservation());
    case personal_context::proto::Entity::kVehicle:
      return PersonalContextVehicleToEntityInstance(entity.vehicle());
    case personal_context::proto::Entity::kOrder:
      return PersonalContextOrderToEntityInstance(entity.order());
    case personal_context::proto::Entity::kShipment:
      return PersonalContextShipmentToEntityInstance(entity.shipment());
    case personal_context::proto::Entity::ENTITY_NOT_SET:
      NOTREACHED();
  }
}

personal_context::proto::EntityType
AutofillEntityTypeToPersonalContextEntityType(EntityType type) {
  switch (type.name()) {
    case EntityTypeName::kOrder:
      return personal_context::proto::EntityType::ORDER;
    case EntityTypeName::kShipment:
      return personal_context::proto::EntityType::SHIPMENT;
    case EntityTypeName::kDriversLicense:
      return personal_context::proto::EntityType::DRIVERS_LICENSE;
    case EntityTypeName::kPassport:
      return personal_context::proto::EntityType::PASSPORT;
    case EntityTypeName::kNationalIdCard:
      return personal_context::proto::EntityType::NATIONAL_ID;
    case EntityTypeName::kFlightReservation:
      return personal_context::proto::EntityType::FLIGHT_RESERVATION;
    case EntityTypeName::kVehicle:
      return personal_context::proto::EntityType::VEHICLE;
    case EntityTypeName::kKnownTravelerNumber:
    case EntityTypeName::kRedressNumber:
      // These entities are not supported by personal context.
      return personal_context::proto::EntityType::UNSPECIFIED;
  }
}

}  // namespace autofill
