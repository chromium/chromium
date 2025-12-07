// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/common/autofill_debug_features.h"
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
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

#if !BUILDFLAG(IS_FUCHSIA)
#include "components/variations/service/google_groups_manager.h"  // nogncheck
#endif  // !BUILDFLAG(IS_FUCHSIA)

namespace autofill {

namespace {

using ::signin::GaiaIdHash;
using ::signin::IdentityManager;
using FeatureCheck = base::FunctionRef<bool(const base::Feature&)>;

// Helper function for debugging why a permissions check failed.
void MaybeOutputReason(std::string* out, std::string_view message) {
  if (out) {
    *out = std::string(message);
  }
}

// Checks whether `country_code` belongs to a country where Wallet is
// supported.
[[nodiscard]] bool IsWalletSupportedCountry(
    const GeoIpCountryCode& country_code) {
  // List of countries where Wallet is supported.
  constexpr static auto kWalletSupportedCountries =
      base::MakeFixedFlatSet<std::string_view>(
          {"AD", "AE", "AF", "AG", "AI", "AL", "AM", "AO", "AQ", "AR", "AS",
           "AT", "AU", "AW", "AX", "AZ", "BA", "BB", "BD", "BE", "BF", "BG",
           "BH", "BI", "BJ", "BL", "BM", "BN", "BO", "BQ", "BR", "BS", "BT",
           "BV", "BW", "BZ", "CA", "CC", "CD", "CF", "CG", "CH", "CI", "CK",
           "CL", "CM", "CO", "CR", "CV", "CW", "CX", "CY", "CZ", "DE", "DJ",
           "DK", "DM", "DO", "EC", "EE", "EG", "EH", "ER", "ES", "ET", "FI",
           "FJ", "FK", "FM", "FO", "FR", "GA", "GB", "GD", "GE", "GF", "GG",
           "GH", "GI", "GL", "GM", "GN", "GP", "GQ", "GR", "GS", "GT", "GU",
           "GW", "GY", "HK", "HM", "HN", "HR", "HT", "HU", "ID", "IE", "IL",
           "IM", "IO", "IQ", "IS", "IT", "JE", "JM", "JO", "JP", "KG", "KH",
           "KI", "KM", "KN", "KW", "KY", "KZ", "LA", "LB", "LC", "LI", "LK",
           "LR", "LS", "LT", "LU", "LV", "MA", "MC", "MD", "ME", "MF", "MG",
           "MH", "MK", "ML", "MN", "MO", "MP", "MQ", "MR", "MS", "MT", "MU",
           "MV", "MW", "MX", "MY", "MZ", "NA", "NC", "NE", "NF", "NG", "NI",
           "NL", "NO", "NP", "NR", "NU", "NZ", "OM", "PA", "PE", "PF", "PG",
           "PH", "PK", "PL", "PM", "PN", "PR", "PS", "PT", "PW", "PY", "QA",
           "RE", "RO", "RS", "RW", "SA", "SB", "SC", "SE", "SG", "SH", "SI",
           "SJ", "SK", "SL", "SM", "SN", "SO", "SR", "ST", "SV", "SX", "SZ",
           "TC", "TD", "TF", "TG", "TH", "TJ", "TK", "TL", "TM", "TN", "TO",
           "TT", "TV", "TW", "TZ", "UA", "UG", "UM", "US", "UY", "VA", "VC",
           "VE", "VG", "VI", "VN", "VU", "WF", "WS", "XK", "YE", "YT", "ZA",
           "ZM", "ZW"});
  if (country_code->empty()) {
    // Assumes a valid country if the country is not set.
    return true;
  }
  return kWalletSupportedCountries.contains(country_code.value());
}

// Checks whether `country_code` belongs to a permitted GeoIp.
[[nodiscard]] bool IsPermittedGeoIp(const GeoIpCountryCode& country_code) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAiIgnoreGeoIp)) {
    return country_code == GeoIpCountryCode("US");
  }

  // Parses `parameter` can returns whether any of the country codes is contains
  // match `country_code`.
  auto contains_geo_ip = [&country_code](std::string_view parameter) {
    return base::Contains(
        base::SplitStringPiece(parameter, ",",
                               base::WhitespaceHandling::TRIM_WHITESPACE,
                               base::SplitResult::SPLIT_WANT_NONEMPTY),
        country_code.value());
  };

  const std::string& allowlist =
      features::kAutofillAiIgnoreGeoIpAllowlist.Get();
  const std::string& blocklist =
      features::kAutofillAiIgnoreGeoIpBlocklist.Get();
  return (blocklist.empty() && allowlist.empty()) ||
         (blocklist.empty() && contains_geo_ip(allowlist)) ||
         (!blocklist.empty() && !contains_geo_ip(blocklist));
}

// Returns the `GaiaIdHash` for the signed in account if there is one or
// `std::nullopt` otherwise.
[[nodiscard]] std::optional<GaiaIdHash> GetAccountGaiaIdHash(
    const IdentityManager* identity_manager) {
  if (!identity_manager) {
    return std::nullopt;
  }
  GaiaId gaia_id =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  if (gaia_id.empty()) {
    return std::nullopt;
  }
  return GaiaIdHash::FromGaiaId(gaia_id);
}

// Returns the default `GaiaIdHash` to use for account-keyed prefs if no user
// is signed in.
[[nodiscard]] GaiaIdHash GetDefaultGaiaIdHash() {
  return {};
}

// Returns whether `action` is relevant for data transparency, i.e. viewing
// and removing data. These are actions that are generally permitted even if
// the AutofillAI is disabled.
[[nodiscard]] bool IsRelevantForDataTransparency(AutofillAiAction action) {
  switch (action) {
    case AutofillAiAction::kAddLocalEntityInstanceInSettings:
    case AutofillAiAction::kCrowdsourcingVote:
    case AutofillAiAction::kFilling:
    case AutofillAiAction::kImport:
    case AutofillAiAction::kIphForOptIn:
    case AutofillAiAction::kLogToMqls:
    case AutofillAiAction::kOptIn:
    case AutofillAiAction::kServerClassificationModel:
    case AutofillAiAction::kUseCachedServerClassificationModelResults:
    case AutofillAiAction::kImportToWallet:
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
                                                AutofillAiAction action,
                                                std::string* debug_message) {
  // Everything requires that `kAutofillAiWithDataSchema` is enabled.
  if (!is_enabled(features::kAutofillAiWithDataSchema)) {
    MaybeOutputReason(debug_message,
                      "AutofillAiWithDataSchema is not enabled.");
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
    case AutofillAiAction::kImportToWallet:
      return is_enabled(features::kAutofillAiWalletVehicleRegistration);
    case AutofillAiAction::kAddLocalEntityInstanceInSettings:
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

// Checks whether all requirements related to syncing state is met.
[[nodiscard]] bool SatisfiesSyncingRequirements(
    AutofillAiAction action,
    const syncer::SyncService* sync_service,
    std::string* debug_message) {
  switch (action) {
    case AutofillAiAction::kImportToWallet:
      return sync_service &&
             sync_service->GetUserSettings()->GetSelectedTypes().Has(
                 syncer::UserSelectableType::kPayments) &&
             sync_service->GetActiveDataTypes().Has(syncer::AUTOFILL_VALUABLE);
    case AutofillAiAction::kIphForOptIn:
    case AutofillAiAction::kServerClassificationModel:
    case AutofillAiAction::kUseCachedServerClassificationModelResults:
    case AutofillAiAction::kAddLocalEntityInstanceInSettings:
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

// Checks if the `entity_type` safistifes specific action requirements.
[[nodiscard]] bool SatisfiesEntityTypeRequirements(
    const AutofillClient& client,
    AutofillAiAction action,
    std::optional<EntityType> entity_type,
    std::string* debug_message) {
  auto entity_type_can_be_upstreamed = [](EntityType type) {
    switch (type.name()) {
      case EntityTypeName::kVehicle:
        return true;
      case EntityTypeName::kFlightReservation:
      case EntityTypeName::kNationalIdCard:
      case EntityTypeName::kPassport:
      case EntityTypeName::kDriversLicense:
      case EntityTypeName::kRedressNumber:
      case EntityTypeName::kKnownTravelerNumber:
        return false;
    }
    NOTREACHED();
  };
  auto entity_type_is_enabled_in_settings = [&](EntityType type) {
    const PrefService* const prefs = client.GetPrefs();
    if (!prefs) {
      MaybeOutputReason(debug_message, "Prefs are not available.");
      return false;
    }
    switch (type.name()) {
      case EntityTypeName::kNationalIdCard:
      case EntityTypeName::kPassport:
      case EntityTypeName::kDriversLicense:
        return prefs->GetBoolean(prefs::kAutofillAiIdentityEntitiesEnabled);
      case EntityTypeName::kVehicle:
      case EntityTypeName::kFlightReservation:
      case EntityTypeName::kRedressNumber:
      case EntityTypeName::kKnownTravelerNumber:
        return prefs->GetBoolean(prefs::kAutofillAiTravelEntitiesEnabled);
    }
    NOTREACHED();
  };
  switch (action) {
    case AutofillAiAction::kImportToWallet:
      CHECK(entity_type) << "An entity type is required to check if an entity "
                            "can be upstreamed";
      return entity_type_can_be_upstreamed(*entity_type);
    case AutofillAiAction::kFilling:
    case AutofillAiAction::kImport:
    case AutofillAiAction::kIphForOptIn:
      CHECK(entity_type)
          << "An entity type is required to check if an entity "
             "can be filled or imported, and IPH requires import";
      return entity_type_is_enabled_in_settings(*entity_type) ||
             !base::FeatureList::IsEnabled(
                 features::kAutofillAiIdentityAndTravelPrefs);
    case AutofillAiAction::kServerClassificationModel:
    case AutofillAiAction::kUseCachedServerClassificationModelResults:
    case AutofillAiAction::kAddLocalEntityInstanceInSettings:
    case AutofillAiAction::kCrowdsourcingVote:
    case AutofillAiAction::kEditAndDeleteEntityInstanceInSettings:
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
                                                   AutofillAiAction action,
                                                   std::string* debug_message) {
  // No pref state can prevent actions that are relevant for data transparency
  // (i.e., showing/updating/removing existing data in settings).
  if (IsRelevantForDataTransparency(action) && has_entity_data_saved) {
    return true;
  }

  const PrefService* const prefs = client.GetPrefs();
  if (!prefs) {
    MaybeOutputReason(debug_message, "Prefs are not available.");
    return false;
  }

  // State of the Address-Autofill pref.
  if (!prefs->GetBoolean(prefs::kAutofillProfileEnabled) &&
      !base::FeatureList::IsEnabled(
          features::kAutofillAiIgnoresWhetherAddressPrefIsEnabled)) {
    MaybeOutputReason(debug_message, "Address Autofill is not enabled.");
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
  // Note that the policy can become disabled even after a user has opted in.
  switch (action) {
    case AutofillAiAction::kAddLocalEntityInstanceInSettings:
    case AutofillAiAction::kCrowdsourcingVote:
    case AutofillAiAction::kEditAndDeleteEntityInstanceInSettings:
    case AutofillAiAction::kFilling:
    case AutofillAiAction::kImport:
    case AutofillAiAction::kLogToMqls:
    case AutofillAiAction::kServerClassificationModel:
    case AutofillAiAction::kUseCachedServerClassificationModelResults:
      return policy_pref_enabled && user_opted_in;
    case AutofillAiAction::kImportToWallet:
      return policy_pref_enabled && user_opted_in &&
             client.IsWalletStorageEnabled();
    case AutofillAiAction::kIphForOptIn:
      // The IPH should only show if the user has not opted in yet.
      return policy_pref_enabled && !user_opted_in;
    case AutofillAiAction::kOptIn:
      if (!policy_pref_enabled) {
        MaybeOutputReason(debug_message, "Address Autofill is not enabled.");
      }
      return policy_pref_enabled;
    case autofill::AutofillAiAction::kListEntityInstancesInSettings:
      return true;
  }
  NOTREACHED();
}

// Checks whether all requirements for `IdentityManager` state are
// met.
[[nodiscard]] bool SatisfiesAccountRequirements(
    const IdentityManager* identity_manager,
    bool has_entity_data_saved,
    AutofillAiAction action,
    std::string* debug_message) {
  if (base::FeatureList::IsEnabled(features::kAutofillAiIgnoreSignInState)) {
    return true;
  }

  if (IsRelevantForDataTransparency(action) && has_entity_data_saved) {
    return true;
  }

  // The user is signed out.
  if (!identity_manager) {
    MaybeOutputReason(debug_message, "User is signed out.");
    return false;
  }

  // The user is only signed in on the web.
  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    MaybeOutputReason(debug_message, "User is signed in only on the web.");
    return false;
  }

  // All other states (sign-in and sync including their paused/error states)
  // are sufficient for us to validate the user's account information.
  const bool result = [&]() {
    if (identity_manager
            ->FindExtendedAccountInfo(identity_manager->GetPrimaryAccountInfo(
                signin::ConsentLevel::kSignin))
            .capabilities.can_use_model_execution_features() ==
        signin::Tribool::kTrue) {
      return true;
    }
    switch (action) {
      case AutofillAiAction::kAddLocalEntityInstanceInSettings:
      case AutofillAiAction::kCrowdsourcingVote:
      case AutofillAiAction::kEditAndDeleteEntityInstanceInSettings:
      case AutofillAiAction::kFilling:
      case AutofillAiAction::kImport:
      case AutofillAiAction::kIphForOptIn:
      case AutofillAiAction::kListEntityInstancesInSettings:
      case AutofillAiAction::kOptIn:
      case AutofillAiAction::kImportToWallet:
        return base::FeatureList::IsEnabled(
            features::kAutofillAiIgnoreCapabilityCheck);
      case AutofillAiAction::kLogToMqls:
      case AutofillAiAction::kServerClassificationModel:
      case AutofillAiAction::kUseCachedServerClassificationModelResults:
        return base::FeatureList::IsEnabled(
            features::kAutofillAiIgnoreCapabilityCheck);
    }
    NOTREACHED();
  }();

  if (!result) {
    MaybeOutputReason(debug_message,
                      "User cannot use model execution features.");
  }
  return result;
}

// Checks whether miscellaneous "other" requirements (OTR, app-locale, Geo-IP)
// are satisfied.
[[nodiscard]] bool SatisfiesMiscellaneousRequirements(
    FeatureCheck is_enabled,
    bool is_off_the_record,
    bool has_entity_data_saved,
    const GeoIpCountryCode& country_code,
    std::string_view app_locale,
    AutofillAiAction action,
    std::string* debug_message) {
  // Off-the-record.
  switch (action) {
    case AutofillAiAction::kAddLocalEntityInstanceInSettings:
    case AutofillAiAction::kCrowdsourcingVote:
    case AutofillAiAction::kEditAndDeleteEntityInstanceInSettings:
    case AutofillAiAction::kImport:
    case AutofillAiAction::kIphForOptIn:
    case AutofillAiAction::kListEntityInstancesInSettings:
    case AutofillAiAction::kLogToMqls:
    case AutofillAiAction::kOptIn:
    case AutofillAiAction::kImportToWallet:
    case AutofillAiAction::kServerClassificationModel: {
      if (is_off_the_record) {
        MaybeOutputReason(debug_message, "Off the record.");
        return false;
      }
      break;
    }
    case AutofillAiAction::kFilling:
    case AutofillAiAction::kUseCachedServerClassificationModelResults:
      // Filling and cache use are permitted when OTR.
      break;
  }

  // Wallet-supported country.
  switch (action) {
    case AutofillAiAction::kImportToWallet:
      if (!IsWalletSupportedCountry(country_code)) {
        return false;
      }
      break;
    case AutofillAiAction::kAddLocalEntityInstanceInSettings:
    case AutofillAiAction::kCrowdsourcingVote:
    case AutofillAiAction::kEditAndDeleteEntityInstanceInSettings:
    case AutofillAiAction::kImport:
    case AutofillAiAction::kIphForOptIn:
    case AutofillAiAction::kListEntityInstancesInSettings:
    case AutofillAiAction::kLogToMqls:
    case AutofillAiAction::kOptIn:
    case AutofillAiAction::kServerClassificationModel:
    case AutofillAiAction::kFilling:
    case AutofillAiAction::kUseCachedServerClassificationModelResults:
      break;
  }

  // App-locale.
  if (app_locale != "en-US" &&
      !base::FeatureList::IsEnabled(features::kAutofillAiIgnoreLocale)) {
    // If the user changes their app-locale, the feature might stop working,
    // but the data should not disappear.
    if (!(IsRelevantForDataTransparency(action) && has_entity_data_saved)) {
      MaybeOutputReason(debug_message, "Unsupported locale.");
      return false;
    }
  }

  // If the user changes their GeoIp, the feature might stop working, but the
  // data should not disappear.
  if (!IsPermittedGeoIp(country_code) &&
      !(IsRelevantForDataTransparency(action) && has_entity_data_saved)) {
    MaybeOutputReason(debug_message, "Unsupported GeoIp.");
    return false;
  }

  return true;
}

}  // namespace

bool MayPerformAutofillAiAction(const AutofillClient& client,
                                AutofillAiAction action,
                                std::optional<EntityType> entity_type,
                                std::string* debug_message) {
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

  if (!SatisfiesFeatureRequirements(feature_check, action, debug_message)) {
    return false;
  }

  const EntityDataManager* const edm = client.GetEntityDataManager();
  if (!edm) {
    MaybeOutputReason(debug_message, "No EDM.");
    return false;
  }
  const bool has_entity_data_saved = !edm->GetEntityInstances().empty();
  if (!SatisfiesPreferenceRequirements(client, has_entity_data_saved, action,
                                       debug_message)) {
    return false;
  }

  if (!SatisfiesAccountRequirements(client.GetIdentityManager(),
                                    has_entity_data_saved, action,
                                    debug_message)) {
    return false;
  }

  if (!SatisfiesSyncingRequirements(action, client.GetSyncService(),
                                    debug_message)) {
    return false;
  }

  if (!SatisfiesEntityTypeRequirements(client, action, entity_type,
                                       debug_message)) {
    return false;
  }

  return SatisfiesMiscellaneousRequirements(
      feature_check, client.IsOffTheRecord(), has_entity_data_saved,
      client.GetVariationConfigCountryCode(), client.GetAppLocale(), action,
      debug_message);
}

bool GetAutofillAiOptInStatus(const AutofillClient& client) {
  return GetAutofillAiOptInStatus(client.GetPrefs(),
                                  client.GetIdentityManager());
}

bool GetAutofillAiOptInStatus(const PrefService* prefs,
                              const signin::IdentityManager* identity_manager) {
  if (!prefs) {
    return false;
  }

  if (base::FeatureList::IsEnabled(features::debug::kAutofillAiForceOptIn)) {
    return true;
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillAiSetSyncablePrefFromAccountPref)) {
    return prefs::IsAutofillAiSyncedOptInStatusEnabled(prefs);
  }

  return GetAutofillAiOptInStatusFromNonSyncingPref(prefs, identity_manager);
}

bool GetAutofillAiOptInStatusFromNonSyncingPref(
    const PrefService* prefs,
    const signin::IdentityManager* identity_manager) {
  if (!prefs) {
    return false;
  }

  // Check the account-independent opt-in setting.
  if (const base::Value* value = syncer::GetAccountKeyedPrefValue(
          prefs, prefs::kAutofillAiOptInStatus, GetDefaultGaiaIdHash());
      value && value->GetIfBool().value_or(false)) {
    return true;
  }

  // Check the account-dependent opt-in setting.
  const std::optional<GaiaIdHash> signed_in_hash =
      GetAccountGaiaIdHash(identity_manager);
  if (!signed_in_hash) {
    return false;
  }
  const base::Value* value = syncer::GetAccountKeyedPrefValue(
      prefs, prefs::kAutofillAiOptInStatus, *signed_in_hash);
  return value && value->GetIfBool().value_or(false);
}

bool SetAutofillAiOptInStatus(AutofillClient& client,
                              AutofillAiOptInStatus opt_in_status) {
  if (!MayPerformAutofillAiAction(client, AutofillAiAction::kOptIn)) {
    return false;
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillAiSetSyncablePrefFromAccountPref)) {
    prefs::SetAutofillAiSyncedOptInStatus(
        client.GetPrefs(), opt_in_status == AutofillAiOptInStatus::kOptedIn);
  }

  // Still set the old pref in case of a rollback.
  const std::optional<GaiaIdHash> signed_in_hash =
      GetAccountGaiaIdHash(client.GetIdentityManager());
  if (signed_in_hash) {
    syncer::SetAccountKeyedPrefValue(
        client.GetPrefs(), prefs::kAutofillAiOptInStatus, *signed_in_hash,
        base::Value(opt_in_status == AutofillAiOptInStatus::kOptedIn));
  }

  // If the user is signed out or is an opt-out, then we need to make sure that
  // it also applies to the pref for the signed out state.
  if (!signed_in_hash || opt_in_status == AutofillAiOptInStatus::kOptedOut) {
    syncer::SetAccountKeyedPrefValue(
        client.GetPrefs(), prefs::kAutofillAiOptInStatus,
        GetDefaultGaiaIdHash(),
        base::Value(opt_in_status == AutofillAiOptInStatus::kOptedIn));
  }

  base::UmaHistogramEnumeration("Autofill.Ai.OptIn.Change", opt_in_status);
  return true;
}

[[nodiscard]] bool HasSetLocalAutofillAiOptInStatus(
    const PrefService* prefs,
    const signin::IdentityManager* identity_manager) {
  const std::optional<GaiaIdHash> signed_in_hash =
      GetAccountGaiaIdHash(identity_manager);
  return syncer::GetAccountKeyedPrefValue(prefs, prefs::kAutofillAiOptInStatus,
                                          GetDefaultGaiaIdHash()) ||
         (signed_in_hash &&
          syncer::GetAccountKeyedPrefValue(prefs, prefs::kAutofillAiOptInStatus,
                                           *signed_in_hash));
}

}  // namespace autofill
