// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/management_utils.h"

#include <string>

#include "base/notreached.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

struct EntityTypeResources {
  int section_title_id = 0;
  int add_entity_id = 0;
  int add_entity_branded_id = 0;
  int edit_entity_id = 0;
  int delete_entity_id = 0;
};

EntityTypeResources GetResourcesForType(EntityTypeName type_name) {
  switch (type_name) {
    case EntityTypeName::kDriversLicense:
      return {
          .section_title_id = IDS_AUTOFILL_AI_DRIVERS_LICENSES_TITLE,
          .add_entity_id = IDS_AUTOFILL_AI_ADD_DRIVERS_LICENSE_ENTITY,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS)
          .add_entity_branded_id = IDS_AUTOFILL_AI_SAVE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_BRANDED,
#endif
          .edit_entity_id = IDS_AUTOFILL_AI_EDIT_DRIVERS_LICENSE_ENTITY,
          .delete_entity_id = IDS_AUTOFILL_AI_DELETE_DRIVERS_LICENSE_ENTITY,
      };
    case EntityTypeName::kKnownTravelerNumber:
      return {
          .section_title_id = IDS_AUTOFILL_AI_KNOWN_TRAVELER_NUMBER_TITLE,
          .add_entity_id = IDS_AUTOFILL_AI_ADD_KNOWN_TRAVELER_NUMBER_ENTITY,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS)
          .add_entity_branded_id =
              IDS_AUTOFILL_AI_SAVE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE_BRANDED,
#endif
          .edit_entity_id = IDS_AUTOFILL_AI_EDIT_KNOWN_TRAVELER_NUMBER_ENTITY,
          .delete_entity_id =
              IDS_AUTOFILL_AI_DELETE_KNOWN_TRAVELER_NUMBER_ENTITY,
      };
    case EntityTypeName::kNationalIdCard:
      return {
          .section_title_id = IDS_AUTOFILL_AI_NATIONAL_IDS_SHORT_TITLE,
          .add_entity_id = IDS_AUTOFILL_AI_ADD_NATIONAL_ID_CARD_ENTITY,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS)
          .add_entity_branded_id =
              IDS_AUTOFILL_AI_SAVE_ID_CARD_ENTITY_DIALOG_TITLE_BRANDED,
#endif
          .edit_entity_id = IDS_AUTOFILL_AI_EDIT_NATIONAL_ID_CARD_ENTITY,
          .delete_entity_id = IDS_AUTOFILL_AI_DELETE_NATIONAL_ID_CARD_ENTITY,
      };
    case EntityTypeName::kPassport:
      return {
          .section_title_id = IDS_AUTOFILL_AI_PASSPORTS_TITLE,
          .add_entity_id = IDS_AUTOFILL_AI_ADD_PASSPORT_ENTITY,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS)
          .add_entity_branded_id =
              IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_BRANDED,
#endif
          .edit_entity_id = IDS_AUTOFILL_AI_EDIT_PASSPORT_ENTITY,
          .delete_entity_id = IDS_AUTOFILL_AI_DELETE_PASSPORT_ENTITY,
      };
    case EntityTypeName::kRedressNumber:
      return {
          .section_title_id = IDS_AUTOFILL_AI_REDRESS_NUMBER_TITLE,
          .add_entity_id = IDS_AUTOFILL_AI_ADD_REDRESS_NUMBER_ENTITY,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS)
          .add_entity_branded_id =
              IDS_AUTOFILL_AI_SAVE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE_BRANDED,
#endif
          .edit_entity_id = IDS_AUTOFILL_AI_EDIT_REDRESS_NUMBER_ENTITY,
          .delete_entity_id = IDS_AUTOFILL_AI_DELETE_REDRESS_NUMBER_ENTITY,
      };
    case EntityTypeName::kVehicle:
      return {
          .section_title_id = IDS_AUTOFILL_AI_VEHICLES_TITLE,
          .add_entity_id = IDS_AUTOFILL_AI_ADD_VEHICLE_ENTITY,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS)
          .add_entity_branded_id =
              IDS_AUTOFILL_AI_SAVE_VEHICLE_ENTITY_DIALOG_TITLE_BRANDED,
#endif
          .edit_entity_id = IDS_AUTOFILL_AI_EDIT_VEHICLE_ENTITY,
          .delete_entity_id = IDS_AUTOFILL_AI_DELETE_VEHICLE_ENTITY,
      };
    case EntityTypeName::kFlightReservation:
      return {
          .section_title_id = IDS_AUTOFILL_AI_FLIGHT_RESERVATIONS_TITLE,
      };
    case EntityTypeName::kOrder:
      return {
          .section_title_id = IDS_AUTOFILL_AI_ORDERS_TITLE,
      };
    case EntityTypeName::kShipment:
      return {
          .section_title_id = IDS_AUTOFILL_AI_SHIPMENTS_TITLE,
      };
  }
  NOTREACHED();
}

std::string GetStringResource(int resource_id) {
  return resource_id == 0 ? std::string()
                          : l10n_util::GetStringUTF8(resource_id);
}

}  // namespace

std::string GetEntityTypeSectionTitleStringForI18n(EntityType entity_type) {
  return GetStringResource(
      GetResourcesForType(entity_type.name()).section_title_id);
}

std::string GetAddEntityTypeStringForI18n(EntityType entity_type,
                                          bool is_wallet_branded) {
  EntityTypeResources resources = GetResourcesForType(entity_type.name());
  return GetStringResource(
      is_wallet_branded && resources.add_entity_branded_id != 0
          ? resources.add_entity_branded_id
          : resources.add_entity_id);
}

std::string GetEditEntityTypeStringForI18n(EntityType entity_type) {
  return GetStringResource(
      GetResourcesForType(entity_type.name()).edit_entity_id);
}

std::string GetDeleteEntityTypeStringForI18n(EntityType entity_type) {
  return GetStringResource(
      GetResourcesForType(entity_type.name()).delete_entity_id);
}

DenseSet<EntityType> GetWritableEntityTypes(
    const GeoIpCountryCode& country_code) {
  DenseSet<EntityType> entity_types;
  for (EntityType entity_type : DenseSet<EntityType>::all()) {
    if (!entity_type.enabled(country_code) || entity_type.read_only()) {
      continue;
    }
    entity_types.insert(entity_type);
  }
  return entity_types;
}

std::vector<EntityInstance> GetEntityInstancesForSettings(
    base::span<const EntityInstance> entities) {
  std::vector<EntityInstance> result;
  for (const EntityInstance& entity : entities) {
    if (entity.record_type() != EntityInstance::RecordType::kPersonalContext) {
      result.push_back(entity);
    }
  }
  return result;
}

}  // namespace autofill
