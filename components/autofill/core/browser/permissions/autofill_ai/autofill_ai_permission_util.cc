// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_util.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_set.h"
#include "base/feature.h"
#include "base/feature_list.h"
#include "base/functional/function_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"
#include "components/sync/base/account_pref_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "google_apis/gaia/gaia_id.h"
#include "url/gurl.h"

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

// Returns true if the account is eligible for Personal Context (e.g. to
// determine whether to show Personal Context settings UI).
[[nodiscard]] bool IsPersonalContextEligible(
    personal_context::PersonalContextEligibilityState state) {
  using personal_context::PersonalContextEligibilityState;
  switch (state) {
    case PersonalContextEligibilityState::kEligible:
      return true;
    case PersonalContextEligibilityState::kDisabledNotEligible:
      return false;
  }
}

// Returns whether the current Android hardware model is configured as eligible
// for Ambient Autofill by feature parameters.
[[nodiscard]] bool IsAndroidDeviceEligibleForAmbientAutofill() {
#if BUILDFLAG(IS_ANDROID)
  const std::string model_name = base::SysInfo::HardwareModelName();
  const std::string enabled_devices_str =
      features::kAutofillAmbientAutofillEnabledDevices.Get();
  return std::ranges::contains(
      base::SplitStringPiece(enabled_devices_str, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY),
      model_name);
#else
  return false;
#endif
}

// Returns the set of supported entity types configured by the feature parameter
// for Ambient Autofill.
DenseSet<EntityType> GetAutofillAmbientAutofillSupportedEntityTypes() {
  const std::string type_list =
      features::kAutofillAmbientAutofillSupportedEntityTypes.Get();

  const std::vector<std::string_view> type_pieces = base::SplitStringPiece(
      type_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  DenseSet<EntityType> supported_types;
  for (std::string_view piece : type_pieces) {
    if (std::optional<EntityType> type = StringToEntityType(piece)) {
      supported_types.insert(*type);
    }
  }
  return supported_types;
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

  // Parses `parameter` and returns whether any of the country codes is contains
  // match `country_code`.
  auto contains_geo_ip = [&country_code](std::string_view parameter) {
    return std::ranges::contains(
        base::SplitStringPiece(parameter, ",",
                               base::WhitespaceHandling::TRIM_WHITESPACE,
                               base::SplitResult::SPLIT_WANT_NONEMPTY),
        country_code.value());
  };

  const std::string& allowlist =
      features::kAutofillAiIgnoreGeoIpAllowlist.Get();
  const std::string& blocklist =
      features::kAutofillAiIgnoreGeoIpBlocklist.Get();
  if (!blocklist.empty()) {
    return !contains_geo_ip(blocklist);
  }
  return allowlist.empty() || contains_geo_ip(allowlist);
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

// Returns whether the `entity_type` is enabled in settings.
[[nodiscard]] bool EntityTypeIsEnabledInSettings(
    const PrefService& prefs,
    std::optional<EntityType> entity_type) {
  switch (entity_type->name()) {
    case EntityTypeName::kNationalIdCard:
    case EntityTypeName::kPassport:
    case EntityTypeName::kDriversLicense:
      return prefs.GetBoolean(prefs::kAutofillAiIdentityEntitiesEnabled);
    case EntityTypeName::kVehicle:
    case EntityTypeName::kFlightReservation:
    case EntityTypeName::kRedressNumber:
    case EntityTypeName::kKnownTravelerNumber:
      return prefs.GetBoolean(prefs::kAutofillAiTravelEntitiesEnabled);
    case EntityTypeName::kOrder:
    case EntityTypeName::kShipment:
      return prefs.GetBoolean(prefs::kAutofillAiShoppingEntitiesEnabled);
  }
  NOTREACHED();
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
    case AutofillAiAction::kLogToMqls:
    case AutofillAiAction::kOptIn:
    case AutofillAiAction::kEnableOrDisable:
    case AutofillAiAction::kServerClassificationModel:
    case AutofillAiAction::kUseCachedServerClassificationModelResults:
    case AutofillAiAction::kImportToWallet:
    case AutofillAiAction::kWalletDataSharingPromotion:
    case AutofillAiAction::kAmbientAutofill:
    case AutofillAiAction::kShowAmbientAutofillInSettings:
    case AutofillAiAction::kTypeSupportsAmbientAutofillData:
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
    FeatureCheck is_enabled,
    bool has_entity_data_saved,
    AutofillAiAction action,
    std::optional<EntityType> entity_type,
    std::string* debug_message) {
  if (IsRelevantForDataTransparency(action) && has_entity_data_saved) {
    return true;
  }

  // Everything requires that `kAutofillAiWithDataSchema` is enabled.
  if (!is_enabled(features::kAutofillAiWithDataSchema)) {
    MaybeOutputReason(debug_message,
                      "AutofillAiWithDataSchema is not enabled.");
    return false;
  }

  auto entity_type_can_be_upstreamed = [&](EntityType type) {
    switch (type.name()) {
      case EntityTypeName::kVehicle:
        return is_enabled(features::kAutofillAiWalletVehicleRegistration);
      case EntityTypeName::kNationalIdCard:
      case EntityTypeName::kPassport:
      case EntityTypeName::kDriversLicense:
      case EntityTypeName::kRedressNumber:
      case EntityTypeName::kKnownTravelerNumber:
        return is_enabled(features::kAutofillAiWalletPrivatePasses);
      case EntityTypeName::kFlightReservation:
      case EntityTypeName::kOrder:
      case EntityTypeName::kShipment:
        return false;
    }
    NOTREACHED();
  };

  switch (action) {
    case AutofillAiAction::kServerClassificationModel:
      return is_enabled(features::kAutofillAiServerModel);
    case AutofillAiAction::kUseCachedServerClassificationModelResults:
      return is_enabled(features::kAutofillAiServerModel) &&
             features::kAutofillAiServerModelUseCacheResults.Get();
    case AutofillAiAction::kImportToWallet:
      CHECK(entity_type) << "An entity type is required to check if an entity "
                            "can be upstreamed";
      return entity_type_can_be_upstreamed(*entity_type);
    case AutofillAiAction::kWalletDataSharingPromotion:
      return is_enabled(features::kAutofillAiWalletVehicleRegistration) ||
             is_enabled(features::kAutofillAiWalletFlightReservation) ||
             is_enabled(features::kAutofillAiWalletPrivatePasses);
    case AutofillAiAction::kAddLocalEntityInstanceInSettings:
    case AutofillAiAction::kCrowdsourcingVote:
    case AutofillAiAction::kEditAndDeleteEntityInstanceInSettings:
    case AutofillAiAction::kFilling:
    case AutofillAiAction::kImport:
    case AutofillAiAction::kListEntityInstancesInSettings:
    case AutofillAiAction::kOptIn:
      return true;
    case AutofillAiAction::kLogToMqls:
      return !is_enabled(features::kAutofillAiUsePrivateAi);
    case AutofillAiAction::kEnableOrDisable:
      return IsAutofillAiDefaultAvailabilityEnabled();
    case AutofillAiAction::kAmbientAutofill:
    case AutofillAiAction::kShowAmbientAutofillInSettings:
    case AutofillAiAction::kTypeSupportsAmbientAutofillData:
      return is_enabled(features::kAutofillAmbientAutofill);
  }
  NOTREACHED();
}

// Checks whether all requirements related to syncing state is met.
[[nodiscard]] bool SatisfiesSyncingRequirements(
    AutofillAiAction action,
    const syncer::SyncService* sync_service) {
  switch (action) {
    case AutofillAiAction::kImportToWallet:
    case AutofillAiAction::kWalletDataSharingPromotion:
      return sync_service &&
             sync_service->GetUserSettings()->GetSelectedTypes().Has(
                 syncer::UserSelectableType::kPayments) &&
             sync_service->GetActiveDataTypes().Has(syncer::AUTOFILL_VALUABLE);
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
    case AutofillAiAction::kEnableOrDisable:
    case AutofillAiAction::kAmbientAutofill:
    case AutofillAiAction::kShowAmbientAutofillInSettings:
    case AutofillAiAction::kTypeSupportsAmbientAutofillData:
      return true;
  }
  NOTREACHED();
}

// Checks whether preference-related requirements are satisfied.
[[nodiscard]] bool SatisfiesPreferenceRequirements(
#if !BUILDFLAG(IS_FUCHSIA)
    const GoogleGroupsManager* google_groups_manager,
#endif
    const PrefService* prefs,
    const signin::IdentityManager* identity_manager,
    bool is_wallet_public_pass_storage_enabled,
    bool has_entity_data_saved,
    AutofillAiAction action,
    std::optional<EntityType> entity_type,
    std::string* debug_message) {
  // No pref state can prevent actions that are relevant for data transparency
  // (i.e., showing/updating/removing existing data in settings).
  if (IsRelevantForDataTransparency(action) && has_entity_data_saved) {
    return true;
  }

  if (!prefs) {
    MaybeOutputReason(debug_message, "Prefs are not available.");
    return false;
  }

  // State of the Address-Autofill pref.
  if (!prefs->GetBoolean(prefs::kAutofillProfileEnabled) &&
      !base::FeatureList::IsEnabled(
          features::kAutofillEnableAutofillSettingsEnterprisePolicy)) {
    MaybeOutputReason(debug_message, "Address Autofill is not enabled.");
    return false;
  }

  const bool policy_pref_enabled =
      !IsAutofillAiDisabledByEnterprisePolicy(prefs);
  const bool personal_context_pref_enabled = prefs->GetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const bool autofill_ai_available =
      GetAutofillAiOptInStatus(prefs, identity_manager) ||
      IsAutofillAiDefaultAvailabilityEnabled();
  // Note that the policy can become disabled even after a user has opted in.
  const bool is_allowed_by_opt_in_or_default =
      IsAutofillAiDefaultAvailabilityEnabled() ||
      (policy_pref_enabled && autofill_ai_available);
#else
  const bool is_allowed_by_opt_in_or_default = true;
#endif
  switch (action) {
    case AutofillAiAction::kLogToMqls:
    case AutofillAiAction::kServerClassificationModel:
      return policy_pref_enabled &&
             GetAutofillAiOptInStatus(prefs, identity_manager);
    case AutofillAiAction::kAddLocalEntityInstanceInSettings:
    case AutofillAiAction::kCrowdsourcingVote:
    case AutofillAiAction::kEditAndDeleteEntityInstanceInSettings:
    case AutofillAiAction::kUseCachedServerClassificationModelResults:
      return is_allowed_by_opt_in_or_default;
    case AutofillAiAction::kFilling:
    case AutofillAiAction::kImport:
      return EntityTypeIsEnabledInSettings(*prefs, *entity_type) &&
             is_allowed_by_opt_in_or_default;
    case AutofillAiAction::kShowAmbientAutofillInSettings:
      return is_allowed_by_opt_in_or_default;
    case AutofillAiAction::kAmbientAutofill:
      return personal_context_pref_enabled && is_allowed_by_opt_in_or_default;
    case AutofillAiAction::kTypeSupportsAmbientAutofillData: {
      if (!entity_type) {
        return false;
      }
      if (!GetAutofillAmbientAutofillSupportedEntityTypes().contains(
              *entity_type)) {
        return false;
      }
      if (!personal_context_pref_enabled) {
        return false;
      }
      return EntityTypeIsEnabledInSettings(*prefs, *entity_type) &&
             is_allowed_by_opt_in_or_default;
    }
    case AutofillAiAction::kImportToWallet:
      if (!EntityTypeIsEnabledInSettings(*prefs, *entity_type)) {
        return false;
      }
      // If `*entity_type` is a public pass, respect
      // `is_wallet_public_pass_storage_enabled`.
      if (!is_wallet_public_pass_storage_enabled &&
          GetWalletPassType(*entity_type,
                            EntityInstance::RecordType::kServerWallet) ==
              EntityInstance::WalletPassType::kPublic) {
        return false;
      }
      return is_allowed_by_opt_in_or_default;
    case AutofillAiAction::kOptIn:
      if (!policy_pref_enabled) {
        MaybeOutputReason(debug_message, "Enterprise policy is not enabled.");
      }
      return policy_pref_enabled;
    case AutofillAiAction::kWalletDataSharingPromotion:
      return !is_wallet_public_pass_storage_enabled &&
             (policy_pref_enabled || IsAutofillAiDefaultAvailabilityEnabled());
    case AutofillAiAction::kEnableOrDisable:
    case AutofillAiAction::kListEntityInstancesInSettings:
      return true;
  }
  NOTREACHED();
}

// Returns the set of eligible subscription tiers configured by feature
// parameters for Ambient Autofill.
base::flat_set<int32_t> GetAutofillAmbientAutofillEligibleTiers() {
  const std::string tier_list =
      features::kAutofillAmbientAutofillEligibleTiers.Get();
  const std::vector<std::string_view> tier_pieces = base::SplitStringPiece(
      tier_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  base::flat_set<int32_t> eligible_tiers;
  eligible_tiers.reserve(tier_pieces.size());
  for (std::string_view piece : tier_pieces) {
    int32_t tier_id = 0;
    if (base::StringToInt(piece, &tier_id)) {
      eligible_tiers.insert(tier_id);
    }
  }
  return eligible_tiers;
}

// Checks whether all requirements for `IdentityManager` state are
// met.
[[nodiscard]] bool SatisfiesAccountRequirements(
    const IdentityManager* identity_manager,
    const subscription_eligibility::SubscriptionEligibilityService*
        subscription_service,
    bool has_entity_data_saved,
    AutofillAiAction action,
    std::optional<EntityType> entity_type,
    std::string* debug_message) {
  if (IsRelevantForDataTransparency(action) && has_entity_data_saved) {
    return true;
  }

  // The user is signed out.
  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    MaybeOutputReason(debug_message, "User not signed into Chrome.");
    return false;
  }

  if (identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          identity_manager->GetPrimaryAccountId(
              signin::ConsentLevel::kSignin))) {
    MaybeOutputReason(debug_message,
                      "User's sign-in is in a persistent error state.");
    return false;
  }
  switch (action) {
    case AutofillAiAction::kImportToWallet: {
      CHECK(entity_type) << "An entity type is required to check if an entity "
                            "can be upstreamed";
      if (GetWalletPassType(*entity_type,
                            EntityInstance::RecordType::kServerWallet) !=
          EntityInstance::WalletPassType::kPrivate) {
        // For non-private passes, there are no additional account requirements.
        break;
      }

      AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
          identity_manager->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin));
      const AccountCapabilities& capabilities =
          account_info.GetAccountCapabilities();
      if (base::FeatureList::IsEnabled(
              features::kAutofillAiWalletPrivatePassesCapability)) {
        if (capabilities.supports_wallet_private_passes_in_autofill() !=
            signin::Tribool::kTrue) {
          MaybeOutputReason(
              debug_message,
              "Account doesn't support private passes in Autofill.");
          return false;
        }
      } else {
        // For private passes, underaged users are not allowed to save. When
        // AutofillAiWalletPrivatePassesCapability is enabled, the
        // supports_wallet_private_passes_in_autofill() capability covers this
        // requirement. Before, it was hackily implemented by relying on another
        // capability.
        if (capabilities.can_use_model_execution_features() !=
            signin::Tribool::kTrue) {
          MaybeOutputReason(debug_message, "User is underaged.");
          return false;
        }
      }
      break;
    }
    case AutofillAiAction::kAmbientAutofill:
    case AutofillAiAction::kShowAmbientAutofillInSettings:
    case AutofillAiAction::kTypeSupportsAmbientAutofillData: {
      if (!IsDeviceOrSubscriptionTierEligibleForAmbientAutofill(
              subscription_service)) {
        MaybeOutputReason(
            debug_message,
            "User subscription tier is not eligible and device is "
            "not eligible.");
        return false;
      }
      break;
    }
    case AutofillAiAction::kOptIn: {
      if (!GetAccountGaiaIdHash(identity_manager).has_value()) {
        return false;
      }
      break;
    }
    case AutofillAiAction::kAddLocalEntityInstanceInSettings:
    case AutofillAiAction::kCrowdsourcingVote:
    case AutofillAiAction::kEditAndDeleteEntityInstanceInSettings:
    case AutofillAiAction::kFilling:
    case AutofillAiAction::kImport:
    case AutofillAiAction::kListEntityInstancesInSettings:
    case AutofillAiAction::kLogToMqls:
    case AutofillAiAction::kEnableOrDisable:
    case AutofillAiAction::kServerClassificationModel:
    case AutofillAiAction::kUseCachedServerClassificationModelResults:
    case AutofillAiAction::kWalletDataSharingPromotion:
      break;
  }
  return true;
}

// Checks whether miscellaneous "other" requirements (OTR, app-locale, Geo-IP)
// are satisfied.
[[nodiscard]] bool SatisfiesMiscellaneousRequirements(
    bool is_off_the_record,
    bool supports_reauth,
    bool has_entity_data_saved,
    const GeoIpCountryCode& country_code,
    personal_context::PersonalContextEligibilityState
        personal_context_eligibility_state,
    AutofillAiAction action,
    std::optional<EntityType> entity_type,
    std::string* debug_message) {
  // Off-the-record.
  switch (action) {
    case AutofillAiAction::kAddLocalEntityInstanceInSettings:
    case AutofillAiAction::kCrowdsourcingVote:
    case AutofillAiAction::kEditAndDeleteEntityInstanceInSettings:
    case AutofillAiAction::kImport:
    case AutofillAiAction::kListEntityInstancesInSettings:
    case AutofillAiAction::kLogToMqls:
    case AutofillAiAction::kOptIn:
    case AutofillAiAction::kEnableOrDisable:
    case AutofillAiAction::kImportToWallet:
    case AutofillAiAction::kWalletDataSharingPromotion:
    case AutofillAiAction::kServerClassificationModel:
    case AutofillAiAction::kAmbientAutofill:
    case AutofillAiAction::kShowAmbientAutofillInSettings:
    case AutofillAiAction::kTypeSupportsAmbientAutofillData: {
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
    case AutofillAiAction::kWalletDataSharingPromotion:
      if (!IsWalletSupportedCountry(country_code)) {
        return false;
      }
      break;
    case AutofillAiAction::kAddLocalEntityInstanceInSettings:
    case AutofillAiAction::kCrowdsourcingVote:
    case AutofillAiAction::kEditAndDeleteEntityInstanceInSettings:
    case AutofillAiAction::kImport:
    case AutofillAiAction::kListEntityInstancesInSettings:
    case AutofillAiAction::kLogToMqls:
    case AutofillAiAction::kOptIn:
    case AutofillAiAction::kEnableOrDisable:
    case AutofillAiAction::kServerClassificationModel:
    case AutofillAiAction::kFilling:
    case AutofillAiAction::kUseCachedServerClassificationModelResults:
    case AutofillAiAction::kAmbientAutofill:
    case AutofillAiAction::kShowAmbientAutofillInSettings:
    case AutofillAiAction::kTypeSupportsAmbientAutofillData:
      break;
  }

  // Personal Context eligibility requirements
  switch (action) {
    case AutofillAiAction::kImportToWallet:
    case AutofillAiAction::kWalletDataSharingPromotion:
    case AutofillAiAction::kAddLocalEntityInstanceInSettings:
    case AutofillAiAction::kCrowdsourcingVote:
    case AutofillAiAction::kEditAndDeleteEntityInstanceInSettings:
    case AutofillAiAction::kImport:
    case AutofillAiAction::kListEntityInstancesInSettings:
    case AutofillAiAction::kLogToMqls:
    case AutofillAiAction::kOptIn:
    case AutofillAiAction::kEnableOrDisable:
    case AutofillAiAction::kServerClassificationModel:
    case AutofillAiAction::kFilling:
    case AutofillAiAction::kUseCachedServerClassificationModelResults:
      break;
    case AutofillAiAction::kShowAmbientAutofillInSettings:
    case AutofillAiAction::kAmbientAutofill:
    case AutofillAiAction::kTypeSupportsAmbientAutofillData: {
      if (!IsPersonalContextEligible(personal_context_eligibility_state)) {
        return false;
      }
      break;
    }
  }

  // Re-auth availability.
  switch (action) {
    case AutofillAiAction::kImportToWallet:
      CHECK(entity_type) << "An entity type is required to check if an entity "
                            "can be upstreamed";
      // Importing Wallet private passes is only supported on devices with
      // re-auth.
      if (!supports_reauth &&
          GetWalletPassType(*entity_type,
                            EntityInstance::RecordType::kServerWallet) ==
              EntityInstance::WalletPassType::kPrivate &&
          !base::FeatureList::IsEnabled(
              features::debug::kAutofillAiDisableReauthRequirement)) {
        return false;
      }
      break;
    case AutofillAiAction::kTypeSupportsAmbientAutofillData:
      CHECK(entity_type) << "An entity type is required to check if an entity "
                            "type supports ambient autofill data";
      if (!supports_reauth &&
          GetPersonalContextSpiiType(
              *entity_type, EntityInstance::RecordType::kPersonalContext) ==
              EntityInstance::PersonalContextSpiiType::kSpii &&
          !base::FeatureList::IsEnabled(
              features::debug::kAutofillAiDisableReauthRequirement)) {
        return false;
      }
      break;
    case AutofillAiAction::kWalletDataSharingPromotion:
    case AutofillAiAction::kAddLocalEntityInstanceInSettings:
    case AutofillAiAction::kCrowdsourcingVote:
    case AutofillAiAction::kEditAndDeleteEntityInstanceInSettings:
    case AutofillAiAction::kImport:
    case AutofillAiAction::kListEntityInstancesInSettings:
    case AutofillAiAction::kLogToMqls:
    case AutofillAiAction::kOptIn:
    case AutofillAiAction::kEnableOrDisable:
    case AutofillAiAction::kServerClassificationModel:
    case AutofillAiAction::kFilling:
    case AutofillAiAction::kUseCachedServerClassificationModelResults:
    case AutofillAiAction::kAmbientAutofill:
    case AutofillAiAction::kShowAmbientAutofillInSettings:
      break;
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
  return MayPerformAutofillAiAction(
#if !BUILDFLAG(IS_FUCHSIA)
      client.GetGoogleGroupsManager(),
#endif
      client.GetPrefs(), client.GetEntityDataManager(),
      client.GetIdentityManager(), client.GetSyncService(),
      client.IsWalletPublicPassStorageEnabled(), client.IsOffTheRecord(),
      client.GetVariationConfigCountryCode(),
      client.GetSubscriptionEligibilityService(),
      client.GetPersonalContextEligibilityState(), action, entity_type,
      debug_message);
}

bool MayPerformAutofillAiAction(
#if !BUILDFLAG(IS_FUCHSIA)
    const GoogleGroupsManager* google_groups_manager,
#endif
    const PrefService* prefs,
    const EntityDataManager* edm,
    const signin::IdentityManager* identity_manager,
    const syncer::SyncService* sync_service,
    bool is_wallet_public_pass_storage_enabled,
    bool is_off_the_record,
    const GeoIpCountryCode& country_code,
    const subscription_eligibility::SubscriptionEligibilityService*
        subscription_service,
    personal_context::PersonalContextEligibilityState
        personal_context_eligibility_state,
    AutofillAiAction action,
    std::optional<EntityType> entity_type,
    std::string* debug_message) {
  auto feature_check = [&](const base::Feature& feature) {
#if !BUILDFLAG(IS_FUCHSIA)
    return google_groups_manager
               ? google_groups_manager->IsFeatureEnabledForProfile(feature)
               : base::FeatureList::IsEnabled(feature);
#else
    return base::FeatureList::IsEnabled(feature);
#endif
  };

  if (!edm) {
    MaybeOutputReason(debug_message, "No EDM.");
    return false;
  }
  const bool has_entity_data_saved = !edm->GetEntityInstances().empty();

  if (!SatisfiesFeatureRequirements(feature_check, has_entity_data_saved,
                                    action, entity_type, debug_message)) {
    return false;
  }

  if (!SatisfiesAccountRequirements(identity_manager, subscription_service,
                                    has_entity_data_saved, action, entity_type,
                                    debug_message)) {
    return false;
  }

  if (!SatisfiesPreferenceRequirements(
#if !BUILDFLAG(IS_FUCHSIA)
          google_groups_manager,
#endif
          prefs, identity_manager, is_wallet_public_pass_storage_enabled,
          has_entity_data_saved, action, entity_type, debug_message)) {
    return false;
  }

  if (!SatisfiesSyncingRequirements(action, sync_service)) {
    return false;
  }

  // If the re-auth availability is unknown, error on the side of caution.
  return SatisfiesMiscellaneousRequirements(
      is_off_the_record, edm->GetReauthAvailability().value_or(false),
      has_entity_data_saved, country_code, personal_context_eligibility_state,
      action, entity_type, debug_message);
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

  if (base::FeatureList::IsEnabled(features::kAutofillAiUsePrivateAi)) {
    return prefs->GetBoolean(prefs::kAutofillAiPrivateInferenceOptInStatus) &&
           !prefs
                ->GetTime(
                    prefs::kAutofillAiPrivateInferenceNoticeShownTimestamp)
                .is_null();
  }

  return GetObsoleteAutofillAiOptInStatus(prefs, identity_manager);
}

bool GetObsoleteAutofillAiOptInStatus(
    const PrefService* prefs,
    const signin::IdentityManager* identity_manager) {
  if (!prefs) {
    return false;
  }

  if (base::FeatureList::IsEnabled(features::debug::kAutofillAiForceOptIn)) {
    return true;
  }

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
  return SetAutofillAiOptInStatus(
#if !BUILDFLAG(IS_FUCHSIA)
      client.GetGoogleGroupsManager(),
#endif
      client.GetPrefs(), client.GetEntityDataManager(),
      client.GetIdentityManager(), client.GetSyncService(),
      client.IsWalletPublicPassStorageEnabled(), client.IsOffTheRecord(),
      client.GetVariationConfigCountryCode(),
      client.GetSubscriptionEligibilityService(),
      client.GetPersonalContextEligibilityState(), opt_in_status);
}

bool SetAutofillAiOptInStatus(
#if !BUILDFLAG(IS_FUCHSIA)
    const GoogleGroupsManager* google_groups_manager,
#endif
    PrefService* prefs,
    const EntityDataManager* edm,
    const signin::IdentityManager* identity_manager,
    const syncer::SyncService* sync_service,
    bool is_wallet_public_pass_storage_enabled,
    bool is_off_the_record,
    const GeoIpCountryCode& country_code,
    const subscription_eligibility::SubscriptionEligibilityService*
        subscription_service,
    personal_context::PersonalContextEligibilityState
        personal_context_eligibility_state,
    AutofillAiOptInStatus opt_in_status) {
  if (!MayPerformAutofillAiAction(
#if !BUILDFLAG(IS_FUCHSIA)
          google_groups_manager,
#endif
          prefs, edm, identity_manager, sync_service,
          is_wallet_public_pass_storage_enabled, is_off_the_record,
          country_code, subscription_service,
          personal_context_eligibility_state, AutofillAiAction::kOptIn)) {
    return false;
  }

  if (base::FeatureList::IsEnabled(features::kAutofillAiUsePrivateAi)) {
    // If the user manually opted-in or out it is because they manually
    // navigated to the settings UI and were shown privacy information about the
    // feature. Therefore, later displaying the the notice is unnecessary.
    prefs->SetTime(prefs::kAutofillAiPrivateInferenceNoticeShownTimestamp,
                   base::Time::Now());
    prefs->SetBoolean(prefs::kAutofillAiPrivateInferenceOptInStatus,
                      opt_in_status == AutofillAiOptInStatus::kOptedIn);
  } else {
    const std::optional<GaiaIdHash> signed_in_hash =
        GetAccountGaiaIdHash(identity_manager);
    // Guaranteed by MayPerformAutofillAiAction(kOptIn).
    CHECK(signed_in_hash.has_value());
    syncer::SetAccountKeyedPrefValue(
        prefs, prefs::kAutofillAiOptInStatus, *signed_in_hash,
        base::Value(opt_in_status == AutofillAiOptInStatus::kOptedIn));
  }

  base::UmaHistogramEnumeration("Autofill.Ai.OptIn.Change", opt_in_status);
  return true;
}

bool IsAutofillAiEntityTypeBlockedByPolicy(const AutofillClient& client,
                                           const GURL& url,
                                           EntityType entity_type) {
  switch (entity_type.name()) {
    case EntityTypeName::kNationalIdCard:
    case EntityTypeName::kPassport:
    case EntityTypeName::kDriversLicense:
      return client.IsAutofillTypeBlockedByPolicy(
          url, AutofillClient::AutofillPolicyDataCategory::kIdentityDocs);
    case EntityTypeName::kVehicle:
    case EntityTypeName::kFlightReservation:
    case EntityTypeName::kRedressNumber:
    case EntityTypeName::kKnownTravelerNumber:
      return client.IsAutofillTypeBlockedByPolicy(
          url, AutofillClient::AutofillPolicyDataCategory::kTravel);
    case EntityTypeName::kOrder:
    case EntityTypeName::kShipment:
      return client.IsAutofillTypeBlockedByPolicy(
          url, AutofillClient::AutofillPolicyDataCategory::kShopping);
  }
  return false;
}

[[nodiscard]] bool IsAutofillAiDisabledByEnterprisePolicy(
    const PrefService* prefs) {
  // State of the AutofillAI-specific enterprise policy pref.
  constexpr int kAutofillPredictionSettingsAllowWithoutLogging =
      std::to_underlying(
          optimization_guide::model_execution::prefs::
              ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging);
  constexpr int kAutofillPredictionSettingsDisabled =
      std::to_underlying(optimization_guide::model_execution::prefs::
                             ModelExecutionEnterprisePolicyValue::kDisable);
  static_assert(kAutofillPredictionSettingsAllowWithoutLogging == 1);
  static_assert(kAutofillPredictionSettingsDisabled == 2);

  const int policy_pref_state = prefs->GetInteger(
      optimization_guide::prefs::
          kAutofillPredictionImprovementsEnterprisePolicyAllowed);
  return policy_pref_state == kAutofillPredictionSettingsDisabled;
}

[[nodiscard]] bool IsAutofillAiAllowedByEnterprisePolicy(
    const PrefService* prefs) {
  // State of the AutofillAI-specific enterprise policy pref.
  constexpr int kAutofillPredictionSettingsAllow =
      std::to_underlying(optimization_guide::model_execution::prefs::
                             ModelExecutionEnterprisePolicyValue::kAllow);
  static_assert(kAutofillPredictionSettingsAllow == 0);

  return prefs->GetInteger(
             optimization_guide::prefs::
                 kAutofillPredictionImprovementsEnterprisePolicyAllowed) ==
         kAutofillPredictionSettingsAllow;
}

bool IsAutofillAiDefaultAvailabilityEnabled() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return base::FeatureList::IsEnabled(features::kAutofillAiAvailableByDefault);
#else
  return true;
#endif
}

[[nodiscard]] bool IsDeviceOrSubscriptionTierEligibleForAmbientAutofill(
    const subscription_eligibility::SubscriptionEligibilityService*
        subscription_eligibility_service) {
  const bool tier_eligible =
      subscription_eligibility_service &&
      GetAutofillAmbientAutofillEligibleTiers().contains(
          subscription_eligibility_service->GetAiSubscriptionTier());
  return tier_eligible || IsAndroidDeviceEligibleForAmbientAutofill();
}

}  // namespace autofill
