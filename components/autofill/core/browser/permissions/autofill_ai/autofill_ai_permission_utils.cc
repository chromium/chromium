// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"

#if !BUILDFLAG(IS_FUCHSIA)
#include "components/variations/service/google_groups_manager.h"  // nogncheck
#endif  // !BUILDFLAG(IS_FUCHSIA)

namespace autofill {

namespace {

// Returns whether `action` is relevant for data transparency, i.e. viewing
// and removing data. These are actions that are generally permitted even if
// the AutofillAI is disabled.
[[nodiscard]] bool IsRelevantForDataTransparency(AutofillAiAction action) {
  switch (action) {
    case AutofillAiAction::kAddEntityInstanceInSettings:
    case AutofillAiAction::kCrowdsourcingVote:
    case AutofillAiAction::kFilling:
    case AutofillAiAction::kImport:
    case AutofillAiAction::kIphForOptIn:
    case AutofillAiAction::kOptIn:
    case AutofillAiAction::kServerClassificationModel:
      return false;
    case AutofillAiAction::kEditAndDeleteEntityInstanceInSettings:
    case AutofillAiAction::kListEntityInstancesInSettings:
      return true;
  }
  NOTREACHED();
}

// Checks whether all requirements for `base::Feature` state are satisfied
// (`kAutofillAiWithDataSchema`, `kAutofillAiServerModel`).
[[nodiscard]] bool SatisfiesFeatureRequirements(
    const GoogleGroupsManager* google_groups_manager,
    AutofillAiAction action) {
  auto is_enabled = [&](const base::Feature& feature) {
#if !BUILDFLAG(IS_FUCHSIA)
    return google_groups_manager
               ? google_groups_manager->IsFeatureEnabledForProfile(feature)
               : base::FeatureList::IsEnabled(feature);
#else
    return base::FeatureList::IsEnabled(feature);
#endif
  };

  // Everything requires that `kAutofillAiWithDataSchema` is enabled.
  if (!is_enabled(features::kAutofillAiWithDataSchema)) {
    return false;
  }

  switch (action) {
    case AutofillAiAction::kServerClassificationModel:
      return is_enabled(features::kAutofillAiServerModel);
    case AutofillAiAction::kAddEntityInstanceInSettings:
    case AutofillAiAction::kCrowdsourcingVote:
    case AutofillAiAction::kEditAndDeleteEntityInstanceInSettings:
    case AutofillAiAction::kFilling:
    case AutofillAiAction::kImport:
    case AutofillAiAction::kIphForOptIn:
      return is_enabled(feature_engagement::kIPHAutofillAiOptInFeature);
    case AutofillAiAction::kListEntityInstancesInSettings:
    case AutofillAiAction::kOptIn:
      return true;
  }
  NOTREACHED();
}

// Checks whether all requirements for `PrefService` state are satisfied.
[[nodiscard]] bool SatisfiesPreferenceRequirements(
    const PrefService& pref_service,
    bool has_entity_data_saved,
    AutofillAiAction action) {
  // No pref state can prevent actions that are relevant for data transparency
  // (i.e., showing/updating/removing existing data in settings).
  if (IsRelevantForDataTransparency(action) && has_entity_data_saved) {
    return true;
  }

  // State of the AutofillAI-specific enterprise policy pref.
  constexpr int kAutofillPredictionSettingsDisabled =
      base::to_underlying(optimization_guide::model_execution::prefs::
                              ModelExecutionEnterprisePolicyValue::kDisable);
  static_assert(kAutofillPredictionSettingsDisabled == 2);
  if (pref_service.GetInteger(
          optimization_guide::prefs::
              kAutofillPredictionImprovementsEnterprisePolicyAllowed) ==
      kAutofillPredictionSettingsDisabled) {
    return false;
  }

  // State of the Address-Autofill pref.
  if (!pref_service.GetBoolean(prefs::kAutofillProfileEnabled)) {
    return false;
  }

  // TODO(crbug.com/397881703): Remove feature guards once the pref is migrated
  // to a GAIA-keyed dictionary. It makes no sense to extend the scope before
  // since we want to remove the old pref anyway.
  // State of the user-set AutofillAI pref.
  const bool autofill_ai_enabled =
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
      pref_service.GetBoolean(prefs::kAutofillPredictionImprovementsEnabled);
#else
      false;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
  switch (action) {
    case AutofillAiAction::kAddEntityInstanceInSettings:
    case AutofillAiAction::kCrowdsourcingVote:
    case AutofillAiAction::kFilling:
    case AutofillAiAction::kImport:
    case AutofillAiAction::kServerClassificationModel:
    case AutofillAiAction::kEditAndDeleteEntityInstanceInSettings:
    case AutofillAiAction::kListEntityInstancesInSettings:
      return autofill_ai_enabled;
    case AutofillAiAction::kIphForOptIn:
      // IPH should only show if the user has not opted in yet.
      return !autofill_ai_enabled;
    case AutofillAiAction::kOptIn:
      return true;
  }
  NOTREACHED();
}

// Checks whether all requirements for `IdentityManager` state are required.
[[nodiscard]] bool SatisfiesAccountRequirements(
    const signin::IdentityManager* identity_manager,
    bool has_entity_data_saved,
    AutofillAiAction action) {
  if (IsRelevantForDataTransparency(action) && has_entity_data_saved) {
    return true;
  }

  // The user is signed out.
  if (!identity_manager) {
    return false;
  }

  // The user is only signed in on the web.
  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return false;
  }

  // All other states (sign-in and sync including their paused/error states)
  // are sufficient for us to validate the user's account information.
  // TODO(crbug.com/397881703): Decide whether to implement overrides similar
  // to `kModelExecutionCapabilityDisable`.
  return identity_manager
             ->FindExtendedAccountInfo(identity_manager->GetPrimaryAccountInfo(
                 signin::ConsentLevel::kSignin))
             .capabilities.can_use_model_execution_features() ==
         signin::Tribool::kTrue;
}

// Checks whether miscellaneous "other" requirements (OTR, app-locale, Geo-IP)
// are satisfied.
[[nodiscard]] bool SatisfiesMiscellaneousRequirements(
    bool is_off_the_record,
    bool has_entity_data_saved,
    const GeoIpCountryCode& country_code,
    std::string_view app_locale,
    AutofillAiAction action) {
  // Off-the-record.
  switch (action) {
    case AutofillAiAction::kAddEntityInstanceInSettings:
    case AutofillAiAction::kCrowdsourcingVote:
    case AutofillAiAction::kEditAndDeleteEntityInstanceInSettings:
    case AutofillAiAction::kImport:
    case AutofillAiAction::kIphForOptIn:
    case AutofillAiAction::kListEntityInstancesInSettings:
    case AutofillAiAction::kOptIn:
    case AutofillAiAction::kServerClassificationModel: {
      if (is_off_the_record) {
        return false;
      }
      break;
    }
    case AutofillAiAction::kFilling:
      // Filling is the only action we permit when OTR.
      break;
  }

  // App-locale.
  if (app_locale != "en-US") {
    // If the user changes their app-locale, the feature might stop working,
    // but the data should not disappear.
    if (!(IsRelevantForDataTransparency(action) && has_entity_data_saved)) {
      return false;
    }
  }

  if (country_code != GeoIpCountryCode("US") &&
      !base::FeatureList::IsEnabled(features::kAutofillAiIgnoreGeoIp)) {
    return false;
  }

  return true;
}

}  // namespace

bool MayPerformAutofillAiAction(const AutofillClient& client,
                                AutofillAiAction action) {
  if (!SatisfiesFeatureRequirements(client.GetGoogleGroupsManager(), action)) {
    return false;
  }

  const EntityDataManager* const edm = client.GetEntityDataManager();
  const PrefService* const prefs = client.GetPrefs();
  if (!edm || !prefs) {
    return false;
  }
  const bool has_entity_data_saved = !edm->GetEntityInstances().empty();
  if (!SatisfiesPreferenceRequirements(*prefs, has_entity_data_saved, action)) {
    return false;
  }

  if (!SatisfiesAccountRequirements(client.GetIdentityManager(),
                                    has_entity_data_saved, action)) {
    return false;
  }

  return SatisfiesMiscellaneousRequirements(
      client.IsOffTheRecord(), has_entity_data_saved,
      client.GetVariationConfigCountryCode(), client.GetAppLocale(), action);
}

}  // namespace autofill
