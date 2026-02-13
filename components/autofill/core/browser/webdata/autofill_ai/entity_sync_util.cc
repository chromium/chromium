// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_ai/entity_sync_util.h"

#include <string>

#include "base/i18n/time_formatting.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/autofill_ai_chrome_metadata.pb.h"
#include "components/autofill/core/browser/webdata/valuables/valuables_sync_util.h"
#include "components/sync/protocol/entity_data.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace autofill {

namespace {

using sync_pb::AutofillValuableMetadataSpecifics;
using sync_pb::AutofillValuableSpecifics;

// Populates or clears a field in the `specifics` proto based on whether the
// corresponding attribute exists in the `entity`.
//
// `proto_field_name` must correspond to the suffix of the set_ and clear_
// methods in the proto (e.g., passing `flight_number` calls `set_flight_number`
// and `clear_flight_number`).
#define SET_OR_CLEAR_STRING_FIELD(entity, attribute_type, proto_field_name, \
                                  specifics)                                \
  if (base::optional_ref<const AttributeInstance> attr =                    \
          entity.attribute(AttributeType(attribute_type))) {                \
    (specifics).set_##proto_field_name(                                     \
        base::UTF16ToUTF8(attr->GetCompleteRawInfo()));                     \
  } else {                                                                  \
    (specifics).clear_##proto_field_name();                                 \
  }

// Wraps a message `m` into an `Any`-typed message, essentially dropping the
// actual type for serialization purposes.
sync_pb::Any AnyWrapProto(const google::protobuf::MessageLite& m) {
  sync_pb::Any any;
  any.set_type_url(base::StrCat({"type.googleapis.com/", m.GetTypeName()}));
  m.SerializeToString(any.mutable_value());
  return any;
}

// Serializes metadata related to `EntityInstance` into
// `ChromeValuablesMetadata`.
sync_pb::Any SerializeChromeValuablesMetadata(const EntityInstance& entity) {
  ChromeValuablesMetadata metadata;
  for (const AttributeInstance& attribute : entity.attributes()) {
    switch (attribute.type().data_type()) {
      case AttributeType::DataType::kName: {
        for (FieldType field_type : attribute.type().field_subtypes()) {
          ChromeValuablesMetadataEntry& entry =
              *metadata.add_metadata_entries();
          entry.set_attribute_type(attribute.type().name_as_string());
          entry.set_field_type(field_type);
          entry.set_value(base::UTF16ToUTF8(attribute.GetRawInfo(field_type)));
          entry.set_verification_status(
              static_cast<int>(attribute.GetVerificationStatus(field_type)));
        }
        break;
      }
      case AttributeType::DataType::kCountry:
      case AttributeType::DataType::kDate:
      case AttributeType::DataType::kState:
      case AttributeType::DataType::kString:
        // Nothing to serialize here as the structure is trivial.
        break;
    }
  }
  return AnyWrapProto(metadata);
}

// Deserializes data in `serialized_metadata` and extends pre-populated
// `attributes` with the information that was serialized. Notably, only
// attributes with equivalent information will be "enriched" to prevent stale
// metadata to override the latest updates. `entity_type` indicates what type of
// entity does the metadata store information for.
void ReadChromeValuablesMetadata(
    base::flat_set<AttributeInstance, AttributeInstance::CompareByType>&
        attributes,
    EntityType entity_type,
    const sync_pb::Any& serialized_metadata) {
  ChromeValuablesMetadata metadata;
  if (!metadata.ParseFromString(serialized_metadata.value())) {
    return;
  }
  base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
      parsed_metadata_attributes;
  for (const ChromeValuablesMetadataEntry& entry :
       metadata.metadata_entries()) {
    std::optional<AttributeType> attribute_type =
        StringToAttributeType(entity_type, entry.attribute_type());
    FieldType field_type = ToSafeFieldType(entry.field_type(), UNKNOWN_TYPE);
    std::u16string value = base::UTF8ToUTF16(entry.value());
    std::optional<VerificationStatus> status =
        ToSafeVerificationStatus(entry.verification_status());
    if (!attribute_type || field_type == UNKNOWN_TYPE || !status) {
      continue;
    }
    auto it = parsed_metadata_attributes.find(*attribute_type);
    AttributeInstance& attribute =
        it != parsed_metadata_attributes.end()
            ? *it
            : *parsed_metadata_attributes
                   .insert(AttributeInstance(*attribute_type))
                   .first;
    attribute.SetRawInfo(field_type, value, *status);
  }

  for (const AttributeInstance& attribute : parsed_metadata_attributes) {
    auto attributes_it = attributes.find(attribute);
    // Merge metadata into existing attributes only when values are identical.
    // This prevents stale metadata from reverting recent updates to the primary
    // fields.
    if (attributes_it != attributes.end() &&
        attributes_it->GetCompleteRawInfo() == attribute.GetCompleteRawInfo()) {
      attributes.erase(attributes_it);
      attributes.insert(attribute);
    }
  }
}

// Adds a string-based attribute to the set.
void AddAttribute(
    AttributeTypeName type,
    const std::string& value,
    base::flat_set<AttributeInstance, AttributeInstance::CompareByType>&
        attributes) {
  AttributeInstance attribute{AttributeType(type)};
  // The VerificationStatus is set to `kNoStatus` because it is irrelevant for
  // string types or will be overwritten by metadata deserialization.
  attribute.SetRawInfo(attribute.type().field_type(), base::UTF8ToUTF16(value),
                       VerificationStatus::kNoStatus);
  attributes.insert(std::move(attribute));
}

// Finalizes the attribute set by reading metadata and calling FinalizeInfo.
void FinalizeEntityAttributes(
    EntityType entity_type,
    const sync_pb::Any& serialized_metadata,
    base::flat_set<AttributeInstance, AttributeInstance::CompareByType>&
        attributes) {
  // Metadata is deserialized last to ensure it enriches existing attributes
  // and overwrites any default statuses set during creation.
  ReadChromeValuablesMetadata(attributes, entity_type, serialized_metadata);
  for (AttributeInstance& attribute : attributes) {
    // Build the attribute's substructures if they don't exist.
    attribute.FinalizeInfo();
  }
}

// Reads the `specifics` message and extract attribute-information from its
// different fields. In particular, it also deserializes the metadata stored in
// the sync message.
base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
GetFlightReservationAttributesFromSpecifics(
    const sync_pb::AutofillValuableSpecifics& specifics) {
  using enum AttributeTypeName;
  CHECK_EQ(specifics.valuable_data_case(),
           sync_pb::AutofillValuableSpecifics::kFlightReservation);
  const sync_pb::FlightReservation& flight_reservation =
      specifics.flight_reservation();
  base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
      attributes;

  AddAttribute(kFlightReservationFlightNumber,
               flight_reservation.flight_number(), attributes);
  AddAttribute(kFlightReservationTicketNumber,
               flight_reservation.flight_ticket_number(), attributes);
  AddAttribute(kFlightReservationConfirmationCode,
               flight_reservation.flight_confirmation_code(), attributes);
  AddAttribute(kFlightReservationPassengerName,
               flight_reservation.passenger_name(), attributes);
  AddAttribute(kFlightReservationDepartureAirport,
               flight_reservation.departure_airport(), attributes);
  AddAttribute(kFlightReservationArrivalAirport,
               flight_reservation.arrival_airport(), attributes);

  if (flight_reservation.has_departure_date_unix_epoch_micros()) {
    // We need to offset the departure time by the departure airport's time zone
    // offset to get the local time of the departure.
    base::Time offsetted_departure_time =
        base::Time::FromMillisecondsSinceUnixEpoch(
            specifics.flight_reservation().departure_date_unix_epoch_micros() /
            1000) +
        base::Seconds(
            flight_reservation.departure_airport_utc_offset_seconds());

    // Departure date is stored in this format to be consistent with how
    // other dates are stored.
    AddAttribute(
        kFlightReservationDepartureDate,
        base::UnlocalizedTimeFormatWithPattern(
            offsetted_departure_time, "yyyy-MM-dd", icu::TimeZone::getGMT()),
        attributes);
  }

  FinalizeEntityAttributes(EntityType(EntityTypeName::kFlightReservation),
                           specifics.serialized_chrome_valuables_metadata(),
                           attributes);
  return attributes;
}

// Takes an `entity` and returns a proto message with the information needed
// in order to send this entity to the sync server.
sync_pb::AutofillValuableSpecifics GetFlightReservationSpecifics(
    const EntityInstance& entity,
    const sync_pb::AutofillValuableSpecifics& base_specifics) {
  using enum AttributeTypeName;
  CHECK_EQ(entity.type().name(), EntityTypeName::kFlightReservation);

  sync_pb::AutofillValuableSpecifics specifics = base_specifics;
  specifics.set_id(*entity.guid());
  specifics.set_is_editable(!entity.are_attributes_read_only());

  sync_pb::FlightReservation& flight_reservation =
      *specifics.mutable_flight_reservation();

  SET_OR_CLEAR_STRING_FIELD(entity, kFlightReservationFlightNumber,
                            flight_number, flight_reservation);
  SET_OR_CLEAR_STRING_FIELD(entity, kFlightReservationTicketNumber,
                            flight_ticket_number, flight_reservation);
  SET_OR_CLEAR_STRING_FIELD(entity, kFlightReservationConfirmationCode,
                            flight_confirmation_code, flight_reservation);
  SET_OR_CLEAR_STRING_FIELD(entity, kFlightReservationPassengerName,
                            passenger_name, flight_reservation);
  SET_OR_CLEAR_STRING_FIELD(entity, kFlightReservationDepartureAirport,
                            departure_airport, flight_reservation);
  SET_OR_CLEAR_STRING_FIELD(entity, kFlightReservationArrivalAirport,
                            arrival_airport, flight_reservation);

  *specifics.mutable_serialized_chrome_valuables_metadata() =
      SerializeChromeValuablesMetadata(entity);
  return specifics;
}

// Reads the `specifics` message and extract attribute-information from its
// different fields. In particular, it also deserializes the metadata stored in
// the sync message.
base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
GetVehicleAttributesFromSpecifics(
    const sync_pb::AutofillValuableSpecifics& specifics) {
  using enum AttributeTypeName;
  CHECK_EQ(specifics.valuable_data_case(),
           sync_pb::AutofillValuableSpecifics::kVehicleRegistration);
  const sync_pb::VehicleRegistration& vehicle =
      specifics.vehicle_registration();
  base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
      attributes;

  AddAttribute(kVehicleMake, vehicle.vehicle_make(), attributes);
  AddAttribute(kVehicleModel, vehicle.vehicle_model(), attributes);
  AddAttribute(kVehicleYear, vehicle.vehicle_year(), attributes);
  AddAttribute(kVehicleVin, vehicle.vehicle_identification_number(),
               attributes);
  AddAttribute(kVehiclePlateNumber, vehicle.vehicle_license_plate(),
               attributes);
  AddAttribute(kVehiclePlateState, vehicle.license_plate_region(), attributes);
  AddAttribute(kVehicleOwner, vehicle.owner_name(), attributes);

  FinalizeEntityAttributes(EntityType(EntityTypeName::kVehicle),
                           specifics.serialized_chrome_valuables_metadata(),
                           attributes);
  return attributes;
}

// Takes an `entity` and returns a proto message with the information needed in
// order to send this entity to the sync server.
sync_pb::AutofillValuableSpecifics GetVehicleInformationSpecifics(
    const EntityInstance& entity,
    const sync_pb::AutofillValuableSpecifics& base_specifics) {
  using enum AttributeTypeName;
  CHECK_EQ(entity.type().name(), EntityTypeName::kVehicle);
  sync_pb::AutofillValuableSpecifics specifics = base_specifics;
  specifics.set_id(*entity.guid());
  specifics.set_is_editable(!entity.are_attributes_read_only());

  sync_pb::VehicleRegistration& vehicle =
      *specifics.mutable_vehicle_registration();

  SET_OR_CLEAR_STRING_FIELD(entity, kVehicleMake, vehicle_make, vehicle);
  SET_OR_CLEAR_STRING_FIELD(entity, kVehicleModel, vehicle_model, vehicle);
  SET_OR_CLEAR_STRING_FIELD(entity, kVehicleYear, vehicle_year, vehicle);
  SET_OR_CLEAR_STRING_FIELD(entity, kVehicleVin, vehicle_identification_number,
                            vehicle);
  SET_OR_CLEAR_STRING_FIELD(entity, kVehiclePlateNumber, vehicle_license_plate,
                            vehicle);
  SET_OR_CLEAR_STRING_FIELD(entity, kVehiclePlateState, license_plate_region,
                            vehicle);
  SET_OR_CLEAR_STRING_FIELD(entity, kVehicleOwner, owner_name, vehicle);

  *specifics.mutable_serialized_chrome_valuables_metadata() =
      SerializeChromeValuablesMetadata(entity);
  return specifics;
}

#undef SET_OR_CLEAR_STRING_FIELD

}  // namespace

std::unique_ptr<syncer::EntityData> CreateEntityDataFromEntityInstance(
    const EntityInstance& entity,
    const sync_pb::AutofillValuableSpecifics& base_specifics) {
  // WARNING: if you are adding support for new `AutofillValuableSpecifics`
  // fields, you need to update the
  // `TrimAutofillValuableSpecificsDataForCaching` function accordingly
  DCHECK_EQ(0u, TrimAutofillValuableSpecificsDataForCaching(
                    CreateSpecificsFromEntityInstance(entity,
                                                      /*base_specifics=*/{}))
                    .ByteSizeLong());

  sync_pb::AutofillValuableSpecifics valuable_specifics =
      CreateSpecificsFromEntityInstance(entity, base_specifics);
  std::unique_ptr<syncer::EntityData> entity_data =
      std::make_unique<syncer::EntityData>();
  entity_data->name = valuable_specifics.id();
  *entity_data->specifics.mutable_autofill_valuable() =
      std::move(valuable_specifics);

  return entity_data;
}

sync_pb::AutofillValuableSpecifics CreateSpecificsFromEntityInstance(
    const EntityInstance& entity,
    const sync_pb::AutofillValuableSpecifics& base_specifics) {
  switch (entity.type().name()) {
    case EntityTypeName::kFlightReservation:
      return GetFlightReservationSpecifics(entity, base_specifics);
    case EntityTypeName::kVehicle:
      return GetVehicleInformationSpecifics(entity, base_specifics);
    case EntityTypeName::kPassport:
    case EntityTypeName::kDriversLicense:
    case EntityTypeName::kNationalIdCard:
    case EntityTypeName::kKnownTravelerNumber:
    case EntityTypeName::kRedressNumber:
      // Those entity types are not synced.
      NOTREACHED();
  }
  NOTREACHED();
}

std::optional<EntityInstance> CreateEntityInstanceFromSpecifics(
    const sync_pb::AutofillValuableSpecifics& specifics) {
  const EntityInstance::EntityId guid(specifics.id());
  switch (specifics.valuable_data_case()) {
    case sync_pb::AutofillValuableSpecifics::kVehicleRegistration: {
      return EntityInstance(
          EntityType(EntityTypeName::kVehicle),
          GetVehicleAttributesFromSpecifics(specifics), guid,
          /*nickname=*/"", /*date_modified=*/{}, /*use_count=*/{},
          /*use_date=*/{}, EntityInstance::RecordType::kServerWallet,
          EntityInstance::AreAttributesReadOnly(!specifics.is_editable()),
          /*frecency_override=*/"");
    }
    case sync_pb::AutofillValuableSpecifics::kFlightReservation: {
      std::string frecency_override;
      if (specifics.flight_reservation()
              .has_departure_date_unix_epoch_micros()) {
        base::Time departure_time = base::Time::FromMillisecondsSinceUnixEpoch(
            specifics.flight_reservation().departure_date_unix_epoch_micros() /
            1000);
        frecency_override = base::TimeFormatAsIso8601(departure_time);
      }
      return EntityInstance(
          EntityType(EntityTypeName::kFlightReservation),
          GetFlightReservationAttributesFromSpecifics(specifics), guid,
          /*nickname=*/"", /*date_modified=*/{}, /*use_count=*/{},
          /*use_date=*/{}, EntityInstance::RecordType::kServerWallet,
          EntityInstance::AreAttributesReadOnly(!specifics.is_editable()),
          frecency_override);
    }
    case sync_pb::AutofillValuableSpecifics::kPassport:
    case sync_pb::AutofillValuableSpecifics::kDriverLicense:
    case sync_pb::AutofillValuableSpecifics::kNationalIdCard:
    case sync_pb::AutofillValuableSpecifics::kRedressNumber:
    case sync_pb::AutofillValuableSpecifics::kKnownTravelerNumber:
      // TODO(crbug.com/481650251): Implement
      return std::nullopt;
    case sync_pb::AutofillValuableSpecifics::kLoyaltyCard:
    case sync_pb::AutofillValuableSpecifics::VALUABLE_DATA_NOT_SET:
      // Such specifics shouldn't reach this function as they aren't supported
      // by AutofillAi.
      return std::nullopt;
  }
  return std::nullopt;
}

std::unique_ptr<syncer::EntityData> CreateEntityDataFromEntityMetadata(
    const EntityInstance::EntityMetadata& metadata,
    const sync_pb::AutofillValuableMetadataSpecifics::PassType pass_type,
    const sync_pb::AutofillValuableMetadataSpecifics& base_specifics) {
  sync_pb::AutofillValuableMetadataSpecifics metadata_specifics =
      CreateSpecificsFromEntityMetadata(metadata, pass_type, base_specifics);
  std::unique_ptr<syncer::EntityData> entity_data =
      std::make_unique<syncer::EntityData>();
  entity_data->name = metadata_specifics.valuable_id();
  *entity_data->specifics.mutable_autofill_valuable_metadata() =
      std::move(metadata_specifics);
  return entity_data;
}

sync_pb::AutofillValuableMetadataSpecifics CreateSpecificsFromEntityMetadata(
    const EntityInstance::EntityMetadata& metadata,
    const sync_pb::AutofillValuableMetadataSpecifics::PassType pass_type,
    const sync_pb::AutofillValuableMetadataSpecifics& base_specifics) {
  sync_pb::AutofillValuableMetadataSpecifics specifics = base_specifics;
  specifics.set_valuable_id(*metadata.guid);
  specifics.set_use_count(metadata.use_count);
  specifics.set_last_used_date_unix_epoch_micros(
      metadata.use_date.ToDeltaSinceWindowsEpoch().InMicroseconds());
  specifics.set_last_modified_date_unix_epoch_micros(
      metadata.date_modified.ToDeltaSinceWindowsEpoch().InMicroseconds());
  specifics.set_pass_type(pass_type);
  return specifics;
}

EntityInstance::EntityMetadata CreateEntityMetadataFromSpecifics(
    const sync_pb::AutofillValuableMetadataSpecifics& specifics) {
  return EntityInstance::EntityMetadata{
      .guid = EntityInstance::EntityId(specifics.valuable_id()),
      .date_modified = base::Time::FromDeltaSinceWindowsEpoch(
          base::Microseconds(specifics.last_modified_date_unix_epoch_micros())),
      .use_count = specifics.use_count(),
      .use_date = base::Time::FromDeltaSinceWindowsEpoch(
          base::Microseconds(specifics.last_used_date_unix_epoch_micros()))};
}

std::optional<sync_pb::AutofillValuableMetadataSpecifics::PassType>
EntityTypeToPassType(EntityType entity_type) {
  switch (entity_type.name()) {
    case EntityTypeName::kFlightReservation:
      return sync_pb::AutofillValuableMetadataSpecifics::FLIGHT_RESERVATION;
    case EntityTypeName::kVehicle:
      return sync_pb::AutofillValuableMetadataSpecifics::VEHICLE_REGISTRATION;
    case EntityTypeName::kPassport:
    case EntityTypeName::kDriversLicense:
    case EntityTypeName::kNationalIdCard:
    case EntityTypeName::kKnownTravelerNumber:
    case EntityTypeName::kRedressNumber:
      // Those entity types are not synced.
      return std::nullopt;
  }
  NOTREACHED();
}

}  // namespace autofill
