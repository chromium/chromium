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
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/account_pref_utils.h"

#if !BUILDFLAG(IS_FUCHSIA)
#include "components/variations/service/google_groups_manager.h"  // nogncheck
#endif  // !BUILDFLAG(IS_FUCHSIA)

namespace autofill {

namespace {

using FeatureCheck = base::FunctionRef<bool(const base::Feature&)>;

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
    case AutofillAiAction::kLogToMqls:
    case AutofillAiAction::kOptIn:
    case AutofillAiAction::kServerClassificationModel:
    case AutofillAiAction::kUseCachedServerClassificationModelResults:
      return false;
    case AutofillAiAction::kEditAndDeleteEntityInstanceInSettings:
    case AutofillAiAction::kListEntityInstancesInSettings:
      return true;
  }
  NOTREACHED();
}

// Checks whether all requirements for `base::Feature` state are satisfied
// (`kAutofillAiWithDataSchema`, `kAutofillAiServerModel`).
[[nodiscard]] bool SatisfiesFeatureRequirements(FeatureCheck is_enabled,
                                                AutofillAiAction action) {
  // Everything requires that `kAutofillAiWithDataSchema` is enabled.
  if (!is_enabled(features::kAutofillAiWithDataSchema)) {
    return false;
  }

  switch (action) {
    case AutofillAiAction::kIphForOptIn:
      return is_enabled(feature_engagement::kIPHAutofillAiOptInFeature);
    case AutofillAiAction::kServerClassificationModel:
      return is_enabled(features::kAutofillAiServerModel);
    case AutofillAiAction::kUseCachedServerClassificationModelResults:
      return is_enabled(features::kAutofillAiServerModel) &&
             features::kAutofillAiServerModelUseCacheResults.Get();
    case AutofillAiAction::kAddEntityInstanceInSettings:
    case AutofillAiAction::kCrowdsourcingVote:
    case AutofillAiAction::kEditAndDeleteEntityInstanceInSettings:
    case AutofillAiAction::kFilling:
    case AutofillAiAction::kImport:
    case AutofillAiAction::kListEntityInstancesInSettings:
    case AutofillAiAction::kLogToMqls:
    case AutofillAiAction::kOptIn:
      return true;
  }
  NOTREACHED();
}

// Checks whether preference-related requirements are satisfied.
[[nodiscard]] bool SatisfiesPreferenceRequirements(const AutofillClient& client,
                                                   bool has_entity_data_saved,
                                                   AutofillAiAction action) {
  // No pref state can prevent actions that are relevant for data transparency
  // (i.e., showing/updating/removing existing data in settings).
  if (IsRelevantForDataTransparency(action) && has_entity_data_saved) {
    return true;
  }

  const PrefService* const prefs = client.GetPrefs();
  if (!prefs) {
    return false;
  }

  // State of the Address-Autofill pref.
  if (!prefs->GetBoolean(prefs::kAutofillProfileEnabled)) {
    return false;
  }

  // State of the AutofillAI-specific enterprise policy pref.
  constexpr int kAutofillPredictionSettingsAllowWithoutLogging =
      base::to_underlying(
          optimization_guide::model_execution::prefs::
              ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging);
  constexpr int kAutofillPredictionSettingsDisabled =
      base::to_underlying(optimization_guide::model_execution::prefs::
                              ModelExecutionEnterprisePolicyValue::kDisable);
  static_assert(kAutofillPredictionSettingsAllowWithoutLogging == 1);
  static_assert(kAutofillPredictionSettingsDisabled == 2);

  const int policy_pref_state = prefs->GetInteger(
      optimization_guide::prefs::
          kAutofillPredictionImprovementsEnterprisePolicyAllowed);
  const bool policy_pref_enabled =
      policy_pref_state != kAutofillPredictionSettingsDisabled;
  const bool user_opted_in = GetAutofillAiOptInStatus(client);
  // Note that the policy can become disabled even after an user has opted in.
  switch (action) {
    case AutofillAiAction::kAddEntityInstanceInSettings:
    case AutofillAiAction::kCrowdsourcingVote:
    case AutofillAiAction::kEditAndDeleteEntityInstanceInSettings:
    case AutofillAiAction::kFilling:
    case AutofillAiAction::kImport:
    case AutofillAiAction::kLogToMqls:
    case AutofillAiAction::kServerClassificationModel:
    case AutofillAiAction::kUseCachedServerClassificationModelResults:
      return policy_pref_enabled && user_opted_in;
    case AutofillAiAction::kIphForOptIn:
      // IPH should only show if the user has not opted in yet.
      return policy_pref_enabled && !user_opted_in;
    case AutofillAiAction::kOptIn:
      return policy_pref_enabled;
    case autofill::AutofillAiAction::kListEntityInstancesInSettings:
      return true;
  }
  NOTREACHED();
}

// Checks whether all requirements for `IdentityManager` state are
// met.
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
    FeatureCheck is_enabled,
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
    case AutofillAiAction::kLogToMqls:
    case AutofillAiAction::kOptIn:
    case AutofillAiAction::kServerClassificationModel: {
      if (is_off_the_record) {
        return false;
      }
      break;
    }
    case AutofillAiAction::kFilling:
    case AutofillAiAction::kUseCachedServerClassificationModelResults:
      // Filling and cache use are permitted when OTR.
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
      !is_enabled(features::kAutofillAiIgnoreGeoIp)) {
    return false;
  }

  return true;
}

}  // namespace

bool MayPerformAutofillAiAction(const AutofillClient& client,
                                AutofillAiAction action) {
#if !BUILDFLAG(IS_FUCHSIA)
  const GoogleGroupsManager* const google_groups_manager =
      client.GetGoogleGroupsManager();
#endif
  auto feature_check = [&](const base::Feature& feature) {
#if !BUILDFLAG(IS_FUCHSIA)
    return google_groups_manager
               ? google_groups_manager->IsFeatureEnabledForProfile(feature)
               : base::FeatureList::IsEnabled(feature);
#else
    return base::FeatureList::IsEnabled(feature);
#endif
  };

  if (!SatisfiesFeatureRequirements(feature_check, action)) {
    return false;
  }

  const EntityDataManager* const edm = client.GetEntityDataManager();
  if (!edm) {
    return false;
  }
  const bool has_entity_data_saved = !edm->GetEntityInstances().empty();
  if (!SatisfiesPreferenceRequirements(client, has_entity_data_saved, action)) {
    return false;
  }

  if (!SatisfiesAccountRequirements(client.GetIdentityManager(),
                                    has_entity_data_saved, action)) {
    return false;
  }

  return SatisfiesMiscellaneousRequirements(
      feature_check, client.IsOffTheRecord(), has_entity_data_saved,
      client.GetVariationConfigCountryCode(), client.GetAppLocale(), action);
}

bool GetAutofillAiOptInStatus(const AutofillClient& client) {
  const PrefService* const prefs = client.GetPrefs();
  const signin::IdentityManager* const identity_manager =
      client.GetIdentityManager();
  if (!prefs || !identity_manager) {
    return false;
  }

  const GaiaId gaia_id =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  if (gaia_id.empty()) {
    return false;
  }

  const base::Value* const value =
      syncer::GetAccountKeyedPrefValue(prefs, prefs::kAutofillAiOptInStatus,
                                       signin::GaiaIdHash::FromGaiaId(gaia_id));
  return value && value->GetIfBool().value_or(false);
}

bool SetAutofillAiOptInStatus(AutofillClient& client, bool opt_in_status) {
  if (!MayPerformAutofillAiAction(client, AutofillAiAction::kOptIn)) {
    return false;
  }

  const GaiaId gaia_id =
      client.GetIdentityManager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  CHECK(!gaia_id.empty());
  syncer::SetAccountKeyedPrefValue(
      client.GetPrefs(), prefs::kAutofillAiOptInStatus,
      signin::GaiaIdHash::FromGaiaId(gaia_id), base::Value(opt_in_status));
  return true;
}

}  // namespace autofill
