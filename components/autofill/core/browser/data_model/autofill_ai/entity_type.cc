// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

#include <optional>

#include "base/feature_list.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/data_model/addresses/contact_info.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

FieldTypeSet AttributeType::storable_field_types(
    base::PassKey<EntityTable> pass_key) const {
  if (data_type() == DataType::kName) {
    return NameInfo::kDatabaseStoredTypes;
  }
  return {field_type()};
}

std::u16string AttributeType::GetNameForI18n() const {
    switch (name()) {
      case AttributeTypeName::kDriversLicenseName:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_DRIVERS_LICENSE_NAME_ATTRIBUTE_NAME);
      case AttributeTypeName::kDriversLicenseState:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_DRIVERS_LICENSE_STATE_ATTRIBUTE_NAME);
      case AttributeTypeName::kDriversLicenseNumber:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_DRIVERS_LICENSE_NUMBER_ATTRIBUTE_NAME);
      case AttributeTypeName::kDriversLicenseExpirationDate:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_DRIVERS_LICENSE_EXPIRATION_DATE_ATTRIBUTE_NAME);
      case AttributeTypeName::kDriversLicenseIssueDate:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_DRIVERS_LICENSE_ISSUE_DATE_ATTRIBUTE_NAME);
      case AttributeTypeName::kFlightReservationFlightNumber:
      case AttributeTypeName::kFlightReservationTicketNumber:
      case AttributeTypeName::kFlightReservationConfirmationCode:
      case AttributeTypeName::kFlightReservationPassengerName:
      case AttributeTypeName::kFlightReservationDepartureAirport:
      case AttributeTypeName::kFlightReservationArrivalAirport:
      case AttributeTypeName::kFlightReservationDepartureDate:
        // Flight reservations are read-only and do not use attribute strings.
        return u"";
      case AttributeTypeName::kKnownTravelerNumberName:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_KNOWN_TRAVELER_NUMBER_NAME_ATTRIBUTE_NAME);
      case AttributeTypeName::kKnownTravelerNumberNumber:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_KNOWN_TRAVELER_NUMBER_ATTRIBUTE_NAME);
      case AttributeTypeName::kKnownTravelerNumberExpirationDate:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE_ATTRIBUTE_NAME);
      case AttributeTypeName::kNationalIdCardName:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_NATIONAL_ID_CARD_NAME_ATTRIBUTE_NAME);
      case AttributeTypeName::kNationalIdCardCountry:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_NATIONAL_ID_CARD_COUNTRY_ATTRIBUTE_NAME);
      case AttributeTypeName::kNationalIdCardNumber:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_NATIONAL_ID_CARD_NUMBER_ATTRIBUTE_NAME);
      case AttributeTypeName::kNationalIdCardIssueDate:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_NATIONAL_ID_CARD_ISSUE_DATE_ATTRIBUTE_NAME);
      case AttributeTypeName::kNationalIdCardExpirationDate:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_NATIONAL_ID_CARD_EXPIRATION_DATE_ATTRIBUTE_NAME);
      case AttributeTypeName::kPassportName:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_PASSPORT_NAME_ATTRIBUTE_NAME);
      case AttributeTypeName::kPassportNumber:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_PASSPORT_NUMBER_ATTRIBUTE_NAME);
      case AttributeTypeName::kPassportCountry:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_PASSPORT_COUNTRY_ATTRIBUTE_NAME);
      case AttributeTypeName::kPassportExpirationDate:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_PASSPORT_EXPIRATION_DATE_ATTRIBUTE_NAME);
      case AttributeTypeName::kPassportIssueDate:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_PASSPORT_ISSUE_DATE_ATTRIBUTE_NAME);
      case AttributeTypeName::kRedressNumberName:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_REDRESS_NUMBER_NAME_ATTRIBUTE_NAME);
      case AttributeTypeName::kRedressNumberNumber:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_REDRESS_NUMBER_ATTRIBUTE_NAME);
      case AttributeTypeName::kVehicleOwner:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_VEHICLE_OWNER_ATTRIBUTE_NAME);
      case AttributeTypeName::kVehiclePlateNumber:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_VEHICLE_PLATE_NUMBER_ATTRIBUTE_NAME);
      case AttributeTypeName::kVehiclePlateState:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_VEHICLE_PLATE_STATE_ATTRIBUTE_NAME);
      case AttributeTypeName::kVehicleVin:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_VEHICLE_VEHICLE_IDENTIFICATION_NUMBER_ATTRIBUTE_NAME);
      case AttributeTypeName::kVehicleMake:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_VEHICLE_MAKE_ATTRIBUTE_NAME);
      case AttributeTypeName::kVehicleModel:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_VEHICLE_MODEL_ATTRIBUTE_NAME);
      case AttributeTypeName::kVehicleYear:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_VEHICLE_YEAR_ATTRIBUTE_NAME);
    }
    NOTREACHED();
}

// static
bool EntityType::ImportOrder(const EntityType& lhs, const EntityType& rhs) {
  auto rank = [](const EntityType& t) constexpr {
    // Lower values indicate a higher priority.
    switch (t.name()) {
      case EntityTypeName::kDriversLicense:
        return 4;
      case EntityTypeName::kFlightReservation:
        return 7;
      case EntityTypeName::kKnownTravelerNumber:
        return 5;
      case EntityTypeName::kNationalIdCard:
        return 2;
      case EntityTypeName::kPassport:
        return 1;
      case EntityTypeName::kRedressNumber:
        return 6;
      case EntityTypeName::kVehicle:
        return 3;
    }
  };
  // For a deterministic behavior, distinct types should have distinct ranks.
  static_assert(
      std::ranges::all_of(DenseSet<EntityType>::all(), [&](EntityType a) {
        return std::ranges::all_of(
            DenseSet<EntityType>::all(),
            [&](EntityType b) { return a == b || rank(a) != rank(b); });
      }));
  return rank(lhs) < rank(rhs);
}

std::u16string EntityType::GetNameForI18n() const {
  switch (name()) {
    case EntityTypeName::kDriversLicense:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_DRIVERS_LICENSE_ENTITY_NAME);
    case EntityTypeName::kFlightReservation:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_FLIGHT_RESERVATION_ENTITY_NAME);
    case EntityTypeName::kKnownTravelerNumber:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_KNOWN_TRAVELER_NUMBER_ENTITY_NAME);
    case EntityTypeName::kNationalIdCard:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_NATIONAL_ID_CARD_ENTITY_NAME);
    case EntityTypeName::kPassport:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_PASSPORT_ENTITY_NAME);
    case EntityTypeName::kRedressNumber:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_REDRESS_NUMBER_ENTITY_NAME);
    case EntityTypeName::kVehicle:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_VEHICLE_ENTITY_NAME);
  }
  NOTREACHED();
}

std::optional<EntityTypeName> ToSafeEntityTypeName(
    std::underlying_type_t<EntityTypeName> raw_value) {
  // We rely here and elsewhere (such as in iteration over
  // `DenseSet<EntityType>::all()`) on the fact that `EntityTypeName` is
  // auto-generated and its value range is contiguous. If it were not, this
  // check would not be sufficient.
  if (raw_value < 0 ||
      raw_value > base::to_underlying(EntityTypeName::kMaxValue)) {
    return std::nullopt;
  }
  return EntityTypeName(raw_value);
}

std::ostream& operator<<(std::ostream& os, AttributeType a) {
  return os << a.name_as_string();
}

std::ostream& operator<<(std::ostream& os, EntityType t) {
  return os << t.name_as_string();
}

}  // namespace autofill
