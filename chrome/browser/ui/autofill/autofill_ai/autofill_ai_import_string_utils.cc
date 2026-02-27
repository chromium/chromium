// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_string_utils.h"

#include <string>

#include "base/feature_list.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

std::u16string GetPromptTitle(EntityTypeName type_name, bool is_save_prompt) {
#if BUILDFLAG(IS_ANDROID)
  if (is_save_prompt) {
    switch (type_name) {
      case EntityTypeName::kDriversLicense:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_ANDROID);
      case EntityTypeName::kKnownTravelerNumber:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE_ANDROID);
      case EntityTypeName::kNationalIdCard:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_NATIONAL_ID_CARD_ENTITY_DIALOG_TITLE_ANDROID);
      case EntityTypeName::kPassport:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID);
      case EntityTypeName::kRedressNumber:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE_ANDROID);
      case EntityTypeName::kVehicle:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_VEHICLE_ENTITY_DIALOG_TITLE_ANDROID);
      case EntityTypeName::kFlightReservation:
        NOTREACHED() << "Entity is read only and doesn't support save prompts.";
      case EntityTypeName::kOrder:
        NOTREACHED() << "Entity is read only and doesn't support save prompts.";
    }
  } else {
    switch (type_name) {
      case EntityTypeName::kDriversLicense:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_ANDROID);
      case EntityTypeName::kKnownTravelerNumber:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE_ANDROID);
      case EntityTypeName::kNationalIdCard:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_NATIONAL_ID_CARD_ENTITY_DIALOG_TITLE_ANDROID);
      case EntityTypeName::kPassport:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID);
      case EntityTypeName::kRedressNumber:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE_ANDROID);
      case EntityTypeName::kVehicle:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_VEHICLE_ENTITY_DIALOG_TITLE_ANDROID);
      case EntityTypeName::kFlightReservation:
        NOTREACHED()
            << "Entity is read only and doesn't support update prompts.";
      case EntityTypeName::kOrder:
        NOTREACHED()
            << "Entity is read only and doesn't support update prompts.";
    }
  }
#else
  if (is_save_prompt) {
    switch (type_name) {
      case EntityTypeName::kDriversLicense:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kKnownTravelerNumber:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kNationalIdCard:
        return l10n_util::GetStringUTF16(
            base::FeatureList::IsEnabled(
                features::kAutofillAiWalletPrivatePasses)
                ? IDS_AUTOFILL_AI_SAVE_ID_CARD_ENTITY_DIALOG_TITLE
                : IDS_AUTOFILL_AI_SAVE_NATIONAL_ID_CARD_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kPassport:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kRedressNumber:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kVehicle:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_VEHICLE_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kFlightReservation:
        NOTREACHED() << "Entity is read only and doesn't support save prompts.";
      case EntityTypeName::kOrder:
        NOTREACHED() << "Entity is read only and doesn't support save prompts.";
    }
  } else {
    switch (type_name) {
      case EntityTypeName::kDriversLicense:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kKnownTravelerNumber:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kNationalIdCard:
        return l10n_util::GetStringUTF16(
            base::FeatureList::IsEnabled(
                features::kAutofillAiWalletPrivatePasses)
                ? IDS_AUTOFILL_AI_UPDATE_ID_CARD_ENTITY_DIALOG_TITLE
                : IDS_AUTOFILL_AI_UPDATE_NATIONAL_ID_CARD_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kPassport:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kRedressNumber:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kVehicle:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_VEHICLE_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kFlightReservation:
        NOTREACHED()
            << "Entity is read only and doesn't support update prompts.";
      case EntityTypeName::kOrder:
        NOTREACHED()
            << "Entity is read only and doesn't support update prompts.";
    }
  }
#endif
  NOTREACHED();
}

std::u16string GetPrimaryButtonText(bool is_save_prompt) {
  return l10n_util::GetStringUTF16(
      is_save_prompt
          ? IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_SAVE_BUTTON
          : IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_UPDATE_DIALOG_UPDATE_BUTTON);
}

}  // namespace autofill
