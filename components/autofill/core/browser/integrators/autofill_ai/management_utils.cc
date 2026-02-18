// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/management_utils.h"

#include "base/notreached.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

std::string GetAddEntityTypeStringForI18n(EntityType entity_type) {
  switch (entity_type.name()) {
    case EntityTypeName::kDriversLicense:
      return l10n_util::GetStringUTF8(
          IDS_AUTOFILL_AI_ADD_DRIVERS_LICENSE_ENTITY);
    case EntityTypeName::kKnownTravelerNumber:
      return l10n_util::GetStringUTF8(
          IDS_AUTOFILL_AI_ADD_KNOWN_TRAVELER_NUMBER_ENTITY);
    case EntityTypeName::kNationalIdCard:
      return l10n_util::GetStringUTF8(
          IDS_AUTOFILL_AI_ADD_NATIONAL_ID_CARD_ENTITY);
    case EntityTypeName::kPassport:
      return l10n_util::GetStringUTF8(IDS_AUTOFILL_AI_ADD_PASSPORT_ENTITY);
    case EntityTypeName::kRedressNumber:
      return l10n_util::GetStringUTF8(
          IDS_AUTOFILL_AI_ADD_REDRESS_NUMBER_ENTITY);
    case EntityTypeName::kVehicle:
      return l10n_util::GetStringUTF8(IDS_AUTOFILL_AI_ADD_VEHICLE_ENTITY);
    case EntityTypeName::kFlightReservation:
      // Flight reservations are read-only and do not use this string.
      return "";
    case EntityTypeName::kOrder:
      // Orders are read-only and do not use this string.
      return "";
  }
  NOTREACHED();
}

std::string GetEditEntityTypeStringForI18n(EntityType entity_type) {
  switch (entity_type.name()) {
    case EntityTypeName::kDriversLicense:
      return l10n_util::GetStringUTF8(
          IDS_AUTOFILL_AI_EDIT_DRIVERS_LICENSE_ENTITY);
    case EntityTypeName::kKnownTravelerNumber:
      return l10n_util::GetStringUTF8(
          IDS_AUTOFILL_AI_EDIT_KNOWN_TRAVELER_NUMBER_ENTITY);
    case EntityTypeName::kNationalIdCard:
      return l10n_util::GetStringUTF8(
          IDS_AUTOFILL_AI_EDIT_NATIONAL_ID_CARD_ENTITY);
    case EntityTypeName::kPassport:
      return l10n_util::GetStringUTF8(IDS_AUTOFILL_AI_EDIT_PASSPORT_ENTITY);
    case EntityTypeName::kRedressNumber:
      return l10n_util::GetStringUTF8(
          IDS_AUTOFILL_AI_EDIT_REDRESS_NUMBER_ENTITY);
    case EntityTypeName::kVehicle:
      return l10n_util::GetStringUTF8(IDS_AUTOFILL_AI_EDIT_VEHICLE_ENTITY);
    case EntityTypeName::kFlightReservation:
      // Flight reservations are read-only and do not use this string.
      return "";
    case EntityTypeName::kOrder:
      // Orders are read-only and do not use this string.
      return "";
  }
  NOTREACHED();
}

std::string GetDeleteEntityTypeStringForI18n(EntityType entity_type) {
  switch (entity_type.name()) {
    case EntityTypeName::kDriversLicense:
      return l10n_util::GetStringUTF8(
          IDS_AUTOFILL_AI_DELETE_DRIVERS_LICENSE_ENTITY);
    case EntityTypeName::kKnownTravelerNumber:
      return l10n_util::GetStringUTF8(
          IDS_AUTOFILL_AI_DELETE_KNOWN_TRAVELER_NUMBER_ENTITY);
    case EntityTypeName::kNationalIdCard:
      return l10n_util::GetStringUTF8(
          IDS_AUTOFILL_AI_DELETE_NATIONAL_ID_CARD_ENTITY);
    case EntityTypeName::kPassport:
      return l10n_util::GetStringUTF8(IDS_AUTOFILL_AI_DELETE_PASSPORT_ENTITY);
    case EntityTypeName::kRedressNumber:
      return l10n_util::GetStringUTF8(
          IDS_AUTOFILL_AI_DELETE_REDRESS_NUMBER_ENTITY);
    case EntityTypeName::kVehicle:
      return l10n_util::GetStringUTF8(IDS_AUTOFILL_AI_DELETE_VEHICLE_ENTITY);
    case EntityTypeName::kFlightReservation:
      // Flight reservations are read-only and do not use this string.
      return "";
    case EntityTypeName::kOrder:
      // Orders are read-only and do not use this string.
      return "";
  }
  NOTREACHED();
}

DenseSet<EntityType> GetWritableEntityTypes(
    const GeoIpCountryCode& country_code) {
  DenseSet<EntityType> entity_types;
  for (EntityType entity_type : autofill::DenseSet<EntityType>::all()) {
    if (!entity_type.enabled(country_code) || entity_type.read_only()) {
      continue;
    }
    entity_types.insert(entity_type);
  }
  return entity_types;
}

}  // namespace autofill
