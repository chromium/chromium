// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/personal_context_conversion_util.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
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

base::Time FormatDateTimeAsTime(
    const personal_context::proto::DateTime& datetime) {
  base::Time::Exploded exploded = {
      .year = datetime.year(),
      .month = datetime.month(),
      .day_of_month = datetime.day(),
      .hour = datetime.hours(),
      .minute = datetime.minutes(),
      .second = datetime.seconds(),
      .millisecond = datetime.nanos() / 1000000,
  };
  base::Time time;
  if (!base::Time::FromUTCExploded(exploded, &time)) {
    return base::Time();
  }
  if (datetime.has_utc_offset()) {
    time -= base::Seconds(datetime.utc_offset().seconds());
  }
  return time;
}

std::u16string FormatDateTimeAsDate(
    const personal_context::proto::DateTime& datetime) {
  if (datetime.year() <= 0 || datetime.month() <= 0 || datetime.day() <= 0) {
    return u"";
  }
  return base::UTF8ToUTF16(base::StringPrintf(
      "%04d-%02d-%02d", datetime.year(), datetime.month(), datetime.day()));
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

std::optional<EntityInstance::PersonalContextRecordTypePayload::Source>
PersonalContextSourceReferenceToSource(
    const personal_context::proto::SourceReference& source_reference) {
  using Source = EntityInstance::PersonalContextRecordTypePayload::Source;
  switch (source_reference.source_reference_case()) {
    case personal_context::proto::SourceReference::kGmail:
      return Source{.type = Source::Type::kGmail,
                    .url = std::string(source_reference.gmail().message_url())};
    case personal_context::proto::SourceReference::kPhotos:
      return Source{.type = Source::Type::kPhotos,
                    .url = std::string(source_reference.photos().photos_url())};
    case personal_context::proto::SourceReference::kDrive:
    case personal_context::proto::SourceReference::SOURCE_REFERENCE_NOT_SET:
      return std::nullopt;
  }
  return std::nullopt;
}

EntityInstance::PersonalContextRecordTypePayload
SourceReferencesToPersonalContextPayload(
    const personal_context::proto::Entity& entity) {
  std::vector<EntityInstance::PersonalContextRecordTypePayload::Source> sources;
  sources.reserve(entity.source_references().size());
  for (const personal_context::proto::SourceReference& source_ref :
       entity.source_references()) {
    if (std::optional<EntityInstance::PersonalContextRecordTypePayload::Source>
            source = PersonalContextSourceReferenceToSource(source_ref)) {
      sources.push_back(std::move(*source));
    }
  }
  return EntityInstance::PersonalContextRecordTypePayload{
      .sources = std::move(sources)};
}

EntityInstance CreateEntityInstance(
    EntityTypeName type_name,
    std::vector<AttributeInstance> attributes,
    const personal_context::proto::Entity& entity,
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
      /*use_date=*/base::Time(),
      SourceReferencesToPersonalContextPayload(entity),
      EntityInstance::AreAttributesReadOnly(true),
      std::move(frecency_override));
}

EntityInstance PersonalContextPassportToEntityInstance(
    const personal_context::proto::Entity& entity,
    std::optional<AttributeInstance::MarkAsMaskedPasskey> passkey) {
  CHECK(entity.has_passport());
  const personal_context::proto::Passport& passport = entity.passport();
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

  return CreateEntityInstance(EntityTypeName::kPassport, std::move(attributes),
                              entity);
}

EntityInstance PersonalContextDriversLicenseToEntityInstance(
    const personal_context::proto::Entity& entity,
    std::optional<AttributeInstance::MarkAsMaskedPasskey> passkey) {
  CHECK(entity.has_drivers_license());
  const personal_context::proto::DriversLicense& dl = entity.drivers_license();
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
                              std::move(attributes), entity);
}

EntityInstance PersonalContextNationalIdToEntityInstance(
    const personal_context::proto::Entity& entity,
    std::optional<AttributeInstance::MarkAsMaskedPasskey> passkey) {
  CHECK(entity.has_national_id());
  const personal_context::proto::NationalId& nid = entity.national_id();
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
                              std::move(attributes), entity);
}

EntityInstance PersonalContextFlightReservationToEntityInstance(
    const personal_context::proto::Entity& entity) {
  CHECK(entity.has_flight_reservation());
  const personal_context::proto::FlightReservation& flight =
      entity.flight_reservation();
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
                 FormatDateTimeAsDate(flight.departure_time()), attributes);
    base::Time departure_time = FormatDateTimeAsTime(flight.departure_time());
    if (!departure_time.is_null()) {
      frecency_override = base::TimeFormatAsIso8601(departure_time);
    }
  }

  return CreateEntityInstance(EntityTypeName::kFlightReservation,
                              std::move(attributes), entity,
                              std::move(frecency_override));
}

EntityInstance PersonalContextVehicleToEntityInstance(
    const personal_context::proto::Entity& entity) {
  CHECK(entity.has_vehicle());
  const personal_context::proto::Vehicle& vehicle = entity.vehicle();
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

  return CreateEntityInstance(EntityTypeName::kVehicle, std::move(attributes),
                              entity);
}

EntityInstance PersonalContextOrderToEntityInstance(
    const personal_context::proto::Entity& entity) {
  CHECK(entity.has_order());
  const personal_context::proto::Order& order = entity.order();
  std::vector<AttributeInstance> attributes;
  AddStringAttribute(kOrderId, order.order_id(), attributes);
  AddStringAttribute(kOrderAccount, order.account(), attributes);
  AddStringAttribute(kOrderMerchantName, order.merchant_name(), attributes);
  AddStringAttribute(kOrderMerchantDomain, order.merchant_domain(), attributes);
  if (order.has_order_date()) {
    AddAttribute(kOrderDate, FormatDate(order.order_date()), attributes);
  }
  if (order.product_names_size() > 0) {
    std::vector<std::string> products(order.product_names().begin(),
                                      order.product_names().end());
    AddStringAttribute(kOrderProductNames, base::JoinString(products, ", "),
                       attributes);
  }

  return CreateEntityInstance(EntityTypeName::kOrder, std::move(attributes),
                              entity);
}

EntityInstance PersonalContextShipmentToEntityInstance(
    const personal_context::proto::Entity& entity) {
  CHECK(entity.has_shipment());
  const personal_context::proto::Shipment& shipment = entity.shipment();
  std::vector<AttributeInstance> attributes;
  AddStringAttribute(kShipmentTrackingNumber, shipment.tracking_number(),
                     attributes);
  AddStringAttribute(kShipmentCarrierName, shipment.carrier_name(), attributes);
  AddStringAttribute(kShipmentCarrierDomain, shipment.carrier_domain(),
                     attributes);
  if (shipment.has_ship_date()) {
    AddAttribute(kShipmentShippedDate, FormatDate(shipment.ship_date()),
                 attributes);
  }
  AddStringAttribute(kShipmentDeliveryZipCode, shipment.delivery_zip_code(),
                     attributes);
  AddStringAttribute(kShipmentMerchantName, shipment.merchant_name(),
                     attributes);
  if (shipment.product_names_size() > 0) {
    const std::vector<std::string> products(shipment.product_names().begin(),
                                            shipment.product_names().end());
    AddStringAttribute(kShipmentProductNames, base::JoinString(products, ", "),
                       attributes);
  }

  return CreateEntityInstance(EntityTypeName::kShipment, std::move(attributes),
                              entity);
}

EntityInstance PersonalContextKnownTravelerNumberToEntityInstance(
    const personal_context::proto::Entity& entity,
    std::optional<AttributeInstance::MarkAsMaskedPasskey> passkey) {
  CHECK(entity.has_known_traveler_number());
  const personal_context::proto::KnownTravelerNumber& ktn =
      entity.known_traveler_number();
  std::vector<AttributeInstance> attributes;
  AddStringAttribute(kKnownTravelerNumberName, ktn.name(), attributes);
  AddStringAttribute(kKnownTravelerNumberNumber, ktn.number(), attributes,
                     passkey);

  return CreateEntityInstance(EntityTypeName::kKnownTravelerNumber,
                              std::move(attributes), entity);
}

}  // namespace

void MaskSpiiEntityFields(personal_context::proto::Entity& entity) {
  auto GetMaskedValue = [](std::string_view value) {
    if (value.empty()) {
      return std::string();
    }
    // Implements ceiling with integer division.
    const size_t suffix_length = std::min<size_t>(4, (value.length() + 3) / 4);
    return std::string(value.substr(value.length() - suffix_length));
  };

  switch (entity.entity_case()) {
    case personal_context::proto::Entity::kPassport:
      entity.mutable_passport()->set_number(
          GetMaskedValue(entity.passport().number()));
      break;
    case personal_context::proto::Entity::kDriversLicense:
      entity.mutable_drivers_license()->set_number(
          GetMaskedValue(entity.drivers_license().number()));
      break;
    case personal_context::proto::Entity::kNationalId:
      entity.mutable_national_id()->set_number(
          GetMaskedValue(entity.national_id().number()));
      break;
    case personal_context::proto::Entity::kKnownTravelerNumber:
      entity.mutable_known_traveler_number()->set_number(
          GetMaskedValue(entity.known_traveler_number().number()));
      break;
    case personal_context::proto::Entity::kOrder:
    case personal_context::proto::Entity::kShipment:
    case personal_context::proto::Entity::kFlightReservation:
    case personal_context::proto::Entity::kVehicle:
    case personal_context::proto::Entity::kSensitivePiiPresence:
    case personal_context::proto::Entity::kEncryptedEntity:
    case personal_context::proto::Entity::ENTITY_NOT_SET:
      break;
  }
}

std::optional<EntityInstance> PersonalContextEntityToEntityInstance(
    const personal_context::proto::Entity& entity,
    bool is_masked) {
  std::optional<AttributeInstance::MarkAsMaskedPasskey> passkey;
  if (is_masked) {
    passkey.emplace(AttributeInstance::MarkAsMaskedPasskey());
  }
  switch (entity.entity_case()) {
    case personal_context::proto::Entity::kPassport:
      return PersonalContextPassportToEntityInstance(entity, passkey);
    case personal_context::proto::Entity::kDriversLicense:
      return PersonalContextDriversLicenseToEntityInstance(entity, passkey);
    case personal_context::proto::Entity::kNationalId:
      return PersonalContextNationalIdToEntityInstance(entity, passkey);
    case personal_context::proto::Entity::kFlightReservation:
      return PersonalContextFlightReservationToEntityInstance(entity);
    case personal_context::proto::Entity::kVehicle:
      return PersonalContextVehicleToEntityInstance(entity);
    case personal_context::proto::Entity::kOrder:
      return PersonalContextOrderToEntityInstance(entity);
    case personal_context::proto::Entity::kShipment:
      return PersonalContextShipmentToEntityInstance(entity);
    case personal_context::proto::Entity::kKnownTravelerNumber:
      return PersonalContextKnownTravelerNumberToEntityInstance(entity,
                                                                passkey);
    case personal_context::proto::Entity::kSensitivePiiPresence:
      return std::nullopt;
    case personal_context::proto::Entity::kEncryptedEntity:
      return std::nullopt;
    case personal_context::proto::Entity::ENTITY_NOT_SET:
      return std::nullopt;
  }
  return std::nullopt;
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
      return personal_context::proto::EntityType::KNOWN_TRAVELER_NUMBER;
    case EntityTypeName::kRedressNumber:
      // These entities are not supported by personal context.
      return personal_context::proto::EntityType::UNSPECIFIED;
  }
}

std::optional<EntityType> ToEntityType(
    personal_context::proto::Entity::EntityCase entity_case) {
  switch (entity_case) {
    case personal_context::proto::Entity::kPassport:
      return EntityType(EntityTypeName::kPassport);
    case personal_context::proto::Entity::kDriversLicense:
      return EntityType(EntityTypeName::kDriversLicense);
    case personal_context::proto::Entity::kNationalId:
      return EntityType(EntityTypeName::kNationalIdCard);
    case personal_context::proto::Entity::kFlightReservation:
      return EntityType(EntityTypeName::kFlightReservation);
    case personal_context::proto::Entity::kVehicle:
      return EntityType(EntityTypeName::kVehicle);
    case personal_context::proto::Entity::kOrder:
      return EntityType(EntityTypeName::kOrder);
    case personal_context::proto::Entity::kShipment:
      return EntityType(EntityTypeName::kShipment);
    case personal_context::proto::Entity::kKnownTravelerNumber:
      return EntityType(EntityTypeName::kKnownTravelerNumber);
    case personal_context::proto::Entity::kSensitivePiiPresence:
    case personal_context::proto::Entity::kEncryptedEntity:
    case personal_context::proto::Entity::ENTITY_NOT_SET:
      return std::nullopt;
  }
}

std::optional<EntityType> ToEntityType(
    personal_context::proto::SensitivePiiPresence::Type presence_type) {
  switch (presence_type) {
    case personal_context::proto::SensitivePiiPresence::DRIVERS_LICENSE:
      return EntityType(EntityTypeName::kDriversLicense);
    case personal_context::proto::SensitivePiiPresence::PASSPORT:
      return EntityType(EntityTypeName::kPassport);
    case personal_context::proto::SensitivePiiPresence::NATIONAL_ID:
      return EntityType(EntityTypeName::kNationalIdCard);
    case personal_context::proto::SensitivePiiPresence::UNSPECIFIED:
      return std::nullopt;
  }
}

}  // namespace autofill
