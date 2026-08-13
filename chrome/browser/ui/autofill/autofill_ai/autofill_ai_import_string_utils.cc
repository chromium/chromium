// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_string_utils.h"

#include <string>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/integrators/autofill_ai/management_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

// Structure holding title resource IDs for all 4 experiment arms:
// - Arm 1: Unbranded title (save_title_id / update_title_id) when flag is
//   disabled or branding is inactive.
// - Arm 2: Default branded title when flag is enabled without a variant.
// - Arm 3: Variant 1 branded title when flag is enabled with variant 1.
// - Arm 4: Variant 2 branded title when flag is enabled with variant 2.
struct TitleResources {
  int save_title_id = 0;
  int save_title_branded_id = 0;
  int update_title_id = 0;
  int update_title_branded_id = 0;
};

TitleResources MakeTitleResources(int save_title_id,
                                  int save_branded_id,
                                  int save_v1_id,
                                  int save_v2_id,
                                  int update_title_id,
                                  int update_branded_id,
                                  int update_v1_id,
                                  int update_v2_id) {
  return {
      .save_title_id = save_title_id,
      .save_title_branded_id = ResolveStringIdsForWalletPass2026Experiment(
          save_branded_id, save_v1_id, save_v2_id),
      .update_title_id = update_title_id,
      .update_title_branded_id = ResolveStringIdsForWalletPass2026Experiment(
          update_branded_id, update_v1_id, update_v2_id),
  };
}

TitleResources GetResourcesForType(EntityTypeName type_name) {
#if BUILDFLAG(IS_ANDROID)
  switch (type_name) {
    case EntityTypeName::kDriversLicense:
      return MakeTitleResources(
          IDS_AUTOFILL_AI_SAVE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_ANDROID,
          IDS_AUTOFILL_AI_SAVE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_ANDROID_BRANDED,
          IDS_AUTOFILL_AI_SAVE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_SAVE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_2_SECURELY,
          IDS_AUTOFILL_AI_UPDATE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_ANDROID,
          IDS_AUTOFILL_AI_UPDATE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_ANDROID_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_2_SECURELY);
    case EntityTypeName::kKnownTravelerNumber:
      return MakeTitleResources(
          IDS_AUTOFILL_AI_SAVE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE_ANDROID,
          IDS_AUTOFILL_AI_SAVE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE_ANDROID_BRANDED,
          IDS_AUTOFILL_AI_SAVE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_SAVE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_2_SECURELY,
          IDS_AUTOFILL_AI_UPDATE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE_ANDROID,
          IDS_AUTOFILL_AI_UPDATE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE_ANDROID_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_2_SECURELY);
    case EntityTypeName::kNationalIdCard:
      return MakeTitleResources(
          IDS_AUTOFILL_AI_SAVE_NATIONAL_ID_CARD_ENTITY_DIALOG_TITLE_ANDROID,
          IDS_AUTOFILL_AI_SAVE_NATIONAL_ID_CARD_ENTITY_DIALOG_TITLE_ANDROID_BRANDED,
          IDS_AUTOFILL_AI_SAVE_NATIONAL_ID_CARD_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_SAVE_NATIONAL_ID_CARD_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_2_SECURELY,
          IDS_AUTOFILL_AI_UPDATE_NATIONAL_ID_CARD_ENTITY_DIALOG_TITLE_ANDROID,
          IDS_AUTOFILL_AI_UPDATE_NATIONAL_ID_CARD_ENTITY_DIALOG_TITLE_ANDROID_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_NATIONAL_ID_CARD_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_NATIONAL_ID_CARD_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_2_SECURELY);
    case EntityTypeName::kPassport:
      return MakeTitleResources(
          IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID,
          IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID_BRANDED,
          IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_2_SECURELY,
          IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID,
          IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_2_SECURELY);
    case EntityTypeName::kRedressNumber:
      return MakeTitleResources(
          IDS_AUTOFILL_AI_SAVE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE_ANDROID,
          IDS_AUTOFILL_AI_SAVE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE_ANDROID_BRANDED,
          IDS_AUTOFILL_AI_SAVE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_SAVE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_2_SECURELY,
          IDS_AUTOFILL_AI_UPDATE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE_ANDROID,
          IDS_AUTOFILL_AI_UPDATE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE_ANDROID_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_2_SECURELY);
    case EntityTypeName::kVehicle:
      return MakeTitleResources(
          IDS_AUTOFILL_AI_SAVE_VEHICLE_ENTITY_DIALOG_TITLE_ANDROID,
          IDS_AUTOFILL_AI_SAVE_VEHICLE_ENTITY_DIALOG_TITLE_ANDROID_BRANDED,
          IDS_AUTOFILL_AI_SAVE_VEHICLE_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_SAVE_VEHICLE_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_2_SECURELY,
          IDS_AUTOFILL_AI_UPDATE_VEHICLE_ENTITY_DIALOG_TITLE_ANDROID,
          IDS_AUTOFILL_AI_UPDATE_VEHICLE_ENTITY_DIALOG_TITLE_ANDROID_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_VEHICLE_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_VEHICLE_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_2_SECURELY);
    case EntityTypeName::kFlightReservation:
    case EntityTypeName::kOrder:
    case EntityTypeName::kShipment:
      NOTREACHED() << "Entity is read only and doesn't support import prompts.";
  }
#else
  switch (type_name) {
    case EntityTypeName::kDriversLicense:
      return MakeTitleResources(
          IDS_AUTOFILL_AI_SAVE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE,
          IDS_AUTOFILL_AI_SAVE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_BRANDED,
          IDS_AUTOFILL_AI_SAVE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_SAVE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_VARIANT_2_SECURELY,
          IDS_AUTOFILL_AI_UPDATE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE,
          IDS_AUTOFILL_AI_UPDATE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_VARIANT_2_SECURELY);
    case EntityTypeName::kKnownTravelerNumber:
      return MakeTitleResources(
          IDS_AUTOFILL_AI_SAVE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE,
          IDS_AUTOFILL_AI_SAVE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE_BRANDED,
          IDS_AUTOFILL_AI_SAVE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_SAVE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE_VARIANT_2_SECURELY,
          IDS_AUTOFILL_AI_UPDATE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE,
          IDS_AUTOFILL_AI_UPDATE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE_VARIANT_2_SECURELY);
    case EntityTypeName::kNationalIdCard: {
      const bool use_private_passes_title = base::FeatureList::IsEnabled(
          features::kAutofillAiWalletPrivatePasses);
      return MakeTitleResources(
          use_private_passes_title
              ? IDS_AUTOFILL_AI_SAVE_ID_CARD_ENTITY_DIALOG_TITLE
              : IDS_AUTOFILL_AI_SAVE_NATIONAL_ID_CARD_ENTITY_DIALOG_TITLE,
          IDS_AUTOFILL_AI_SAVE_ID_CARD_ENTITY_DIALOG_TITLE_BRANDED,
          IDS_AUTOFILL_AI_SAVE_ID_CARD_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_SAVE_ID_CARD_ENTITY_DIALOG_TITLE_VARIANT_2_SECURELY,
          use_private_passes_title
              ? IDS_AUTOFILL_AI_UPDATE_ID_CARD_ENTITY_DIALOG_TITLE
              : IDS_AUTOFILL_AI_UPDATE_NATIONAL_ID_CARD_ENTITY_DIALOG_TITLE,
          IDS_AUTOFILL_AI_UPDATE_ID_CARD_ENTITY_DIALOG_TITLE_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_ID_CARD_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_ID_CARD_ENTITY_DIALOG_TITLE_VARIANT_2_SECURELY);
    }
    case EntityTypeName::kPassport:
      return MakeTitleResources(
          IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE,
          IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_BRANDED,
          IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_VARIANT_2_SECURELY,
          IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE,
          IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE_VARIANT_2_SECURELY);
    case EntityTypeName::kRedressNumber:
      return MakeTitleResources(
          IDS_AUTOFILL_AI_SAVE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE,
          IDS_AUTOFILL_AI_SAVE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE_BRANDED,
          IDS_AUTOFILL_AI_SAVE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_SAVE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE_VARIANT_2_SECURELY,
          IDS_AUTOFILL_AI_UPDATE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE,
          IDS_AUTOFILL_AI_UPDATE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE_VARIANT_2_SECURELY);
    case EntityTypeName::kVehicle:
      return MakeTitleResources(
          IDS_AUTOFILL_AI_SAVE_VEHICLE_ENTITY_DIALOG_TITLE,
          IDS_AUTOFILL_AI_SAVE_VEHICLE_ENTITY_DIALOG_TITLE_BRANDED,
          IDS_AUTOFILL_AI_SAVE_VEHICLE_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_SAVE_VEHICLE_ENTITY_DIALOG_TITLE_VARIANT_2_SECURELY,
          IDS_AUTOFILL_AI_UPDATE_VEHICLE_ENTITY_DIALOG_TITLE,
          IDS_AUTOFILL_AI_UPDATE_VEHICLE_ENTITY_DIALOG_TITLE_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_VEHICLE_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED,
          IDS_AUTOFILL_AI_UPDATE_VEHICLE_ENTITY_DIALOG_TITLE_VARIANT_2_SECURELY);
    case EntityTypeName::kFlightReservation:
    case EntityTypeName::kOrder:
    case EntityTypeName::kShipment:
      NOTREACHED() << "Entity is read only and doesn't support import prompts.";
  }
#endif
}

}  // namespace

std::u16string GetPromptTitle(EntityTypeName type_name,
                              bool is_save_prompt,
                              bool is_banner_prompt,
                              bool is_server_wallet) {
  const bool is_wallet_branded =
      !is_banner_prompt && is_server_wallet &&
      base::FeatureList::IsEnabled(features::kAutofillAiWalletPassBranding2026);

  TitleResources resources = GetResourcesForType(type_name);

  int resource_id = 0;
  if (is_save_prompt) {
    resource_id = is_wallet_branded ? resources.save_title_branded_id
                                    : resources.save_title_id;
  } else {
    resource_id = is_wallet_branded ? resources.update_title_branded_id
                                    : resources.update_title_id;
  }

  return l10n_util::GetStringUTF16(resource_id);
}

int GetPrimaryButtonTextId(bool is_save_prompt) {
  return is_save_prompt
             ? IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_SAVE_BUTTON
             : IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_UPDATE_DIALOG_UPDATE_BUTTON;
}

}  // namespace autofill
