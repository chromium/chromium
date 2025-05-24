// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/studies/autofill_experiments.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_settings_metrics.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/variations/variations_associated_data.h"
#include "crypto/sha2.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace autofill {
namespace {

void LogCardUploadDisabled(LogManager* log_manager, std::string_view context) {
  LOG_AF(log_manager) << LoggingScope::kCreditCardUploadStatus
                      << LogMessage::kCreditCardUploadDisabled << context
                      << CTag{};
}

void LogCardUploadEnabled(LogManager* log_manager) {
  LOG_AF(log_manager) << LoggingScope::kCreditCardUploadStatus
                      << LogMessage::kCreditCardUploadEnabled << CTag{};
}

// Returns the opt-in bitfield for the specific |account_hash| or 0 if no entry
// was found.
int GetSyncTransportOptInBitFieldForAccount(const PrefService* prefs,
                                            const std::string& account_hash) {
  const auto& dictionary = prefs->GetDict(prefs::kAutofillSyncTransportOptIn);

  // If there is no entry in the dictionary, it means the account didn't opt-in.
  // Use 0 because it's the same as not having opted-in to anything.
  const auto found = dictionary.FindInt(account_hash);
  return found.value_or(0);
}

}  // namespace

// The list of countries for which the credit card upload save feature is fully
// launched. Last updated M129.
const char* const kAutofillUpstreamLaunchedCountries[] = {
    "AD", "AE", "AF", "AG", "AI", "AL", "AO", "AR", "AS", "AT", "AU", "AW",
    "AZ", "BA", "BB", "BE", "BF", "BG", "BH", "BJ", "BM", "BN", "BR", "BS",
    "BT", "BW", "BZ", "CA", "CD", "CF", "CG", "CH", "CI", "CK", "CL", "CM",
    "CO", "CR", "CV", "CX", "CY", "CZ", "DE", "DJ", "DK", "DM", "DO", "EC",
    "EE", "EH", "ER", "ES", "FI", "FJ", "FK", "FM", "FO", "FR", "GA", "GB",
    "GD", "GE", "GF", "GH", "GI", "GL", "GM", "GN", "GP", "GQ", "GR", "GT",
    "GU", "GW", "GY", "HK", "HN", "HR", "HT", "HU", "IE", "IL", "IO", "IS",
    "IT", "JP", "KE", "KH", "KI", "KM", "KN", "KW", "KY", "KZ", "LA", "LC",
    "LI", "LK", "LR", "LS", "LT", "LU", "LV", "MC", "MD", "ME", "MG", "MH",
    "MK", "ML", "MN", "MO", "MP", "MQ", "MR", "MS", "MT", "MU", "MW", "MX",
    "MY", "MZ", "NA", "NC", "NE", "NF", "NG", "NI", "NL", "NO", "NR", "NZ",
    "OM", "PA", "PE", "PF", "PG", "PH", "PL", "PM", "PR", "PT", "PW", "PY",
    "QA", "RE", "RO", "SB", "SC", "SE", "SG", "SI", "SJ", "SK", "SL", "SM",
    "SN", "SR", "ST", "SV", "SZ", "TC", "TD", "TG", "TH", "TL", "TM", "TO",
    "TR", "TT", "TV", "TW", "TZ", "UA", "UG", "US", "UY", "VC", "VE", "VG",
    "VI", "VN", "VU", "WS", "YT", "ZA", "ZM", "ZW"};

bool IsCreditCardUploadEnabled(
    const syncer::SyncService* sync_service,
    const PrefService& pref_service,
    const std::string& user_country,
    AutofillMetrics::PaymentsSigninState signin_state_for_metrics,
    LogManager* log_manager) {
  if (!sync_service) {
    // If credit card sync is not active, we're not offering to upload cards.
    autofill_metrics::LogCardUploadEnabledMetric(
        autofill_metrics::CardUploadEnabled::kSyncServiceNull,
        signin_state_for_metrics);
    LogCardUploadDisabled(log_manager, "SYNC_SERVICE_NULL");
    return false;
  }

  if (sync_service->GetTransportState() ==
      syncer::SyncService::TransportState::PAUSED) {
    autofill_metrics::LogCardUploadEnabledMetric(
        autofill_metrics::CardUploadEnabled::kSyncServicePaused,
        signin_state_for_metrics);
    LogCardUploadDisabled(log_manager, "SYNC_SERVICE_PAUSED");
    return false;
  }

  if (!sync_service->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA)) {
    autofill_metrics::LogCardUploadEnabledMetric(
        autofill_metrics::CardUploadEnabled::
            kSyncServiceMissingAutofillWalletDataActiveType,
        signin_state_for_metrics);
    LogCardUploadDisabled(
        log_manager, "SYNC_SERVICE_MISSING_AUTOFILL_WALLET_ACTIVE_DATA_TYPE");
    // Log the specific reason sync was not active. Note that this is
    // best-effort, as Sync also takes ~10 seconds for the data types to become
    // active after starting the browser.
    autofill_metrics::SyncDisabledReason reason;
    if (sync_service->GetDisableReasons().Has(
            syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN)) {
      reason = autofill_metrics::SyncDisabledReason::kNotSignedIn;
    } else if (sync_service->GetDisableReasons().Has(
                   syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY)) {
      reason = autofill_metrics::SyncDisabledReason::kSyncDisabledByPolicy;
    } else if (sync_service->GetUserSettings()->GetSelectedTypes().Has(
                   syncer::UserSelectableType::kPayments)) {
      // A mismatch between the kPayments type being "selected" vs. "active".
      // Should be rare due to checking if Sync was active above, but good to
      // check to prove these values are generally interchangeable for our case.
      reason = autofill_metrics::SyncDisabledReason::kSelectedButNotActive;
    } else if (sync_service->GetUserSettings()->IsTypeManagedByPolicy(
                   syncer::UserSelectableType::kPayments)) {
      reason = autofill_metrics::SyncDisabledReason::kTypeDisabledByPolicy;
    } else if (sync_service->GetUserSettings()->IsTypeManagedByCustodian(
                   syncer::UserSelectableType::kPayments)) {
      reason = autofill_metrics::SyncDisabledReason::kTypeDisabledByCustodian;
    } else {
      // By process of elimination, if the toggle is not disabled for any of the
      // above reasons, it is reasonable to expect that it was due to an
      // explicit choice by the user.
      reason =
          autofill_metrics::SyncDisabledReason::kTypeProbablyDisabledByUser;
    }
    LogAutofillPaymentsSyncDisabled(reason);
    return false;
  }

  // In sync settings, address and payment toggles are independent. However,
  // since address information is uploaded during the server card saving flow,
  // credit card upload is not available when address sync is disabled.
  // Before address sync was available in transport mode, server card save was
  // offered in transport mode regardless of the setting. (The sync API exposes
  // the kAutofill type as disabled in this case.)
  // TODO(crbug.com/40066949): Simplify once IsSyncFeatureActive() is deleted
  // from the codebase.
  bool addresses_in_transport_mode = true;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Dice users don't have addresses in transport mode until they went through
  // the explicit signin flow.
  addresses_in_transport_mode =
      pref_service.GetBoolean(::prefs::kExplicitBrowserSignin);
#endif
  bool syncing_or_addresses_in_transport_mode =
      sync_service->IsSyncFeatureActive() || addresses_in_transport_mode;
  if (syncing_or_addresses_in_transport_mode &&
      !sync_service->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kAutofill)) {
    autofill_metrics::LogCardUploadEnabledMetric(
        autofill_metrics::CardUploadEnabled::
            kSyncServiceMissingAutofillSelectedType,
        signin_state_for_metrics);
    LogCardUploadDisabled(log_manager,
                          "SYNC_SERVICE_MISSING_AUTOFILL_SELECTED_TYPE");
    return false;
  }

  // Also don't offer upload for users that have an explicit sync passphrase.
  // Users who have enabled a passphrase have chosen to not make their sync
  // information accessible to Google. Since upload makes credit card data
  // available to other Google systems, disable it for passphrase users.
  if (sync_service->GetUserSettings()->IsUsingExplicitPassphrase()) {
    autofill_metrics::LogCardUploadEnabledMetric(
        autofill_metrics::CardUploadEnabled::kUsingExplicitSyncPassphrase,
        signin_state_for_metrics);
    LogCardUploadDisabled(log_manager, "USER_HAS_EXPLICIT_SYNC_PASSPHRASE");
    return false;
  }

  // Don't offer upload for users that are only syncing locally, since they
  // won't receive the cards back from Google Payments.
  if (sync_service->IsLocalSyncEnabled()) {
    autofill_metrics::LogCardUploadEnabledMetric(
        autofill_metrics::CardUploadEnabled::kLocalSyncEnabled,
        signin_state_for_metrics);
    LogCardUploadDisabled(log_manager, "USER_ONLY_SYNCING_LOCALLY");
    return false;
  }

  if (base::FeatureList::IsEnabled(features::kAutofillUpstream)) {
    // Feature flag is enabled, so continue regardless of the country. This is
    // required for the ability to continue to launch to more countries as
    // necessary.
    autofill_metrics::LogCardUploadEnabledMetric(
        autofill_metrics::CardUploadEnabled::kEnabledByFlag,
        signin_state_for_metrics);
    LogCardUploadEnabled(log_manager);
    return true;
  }

  std::string country_code = base::ToUpperASCII(user_country);
  auto* const* country_iter =
      std::ranges::find(kAutofillUpstreamLaunchedCountries, country_code);
  if (country_iter == std::end(kAutofillUpstreamLaunchedCountries)) {
    // |country_code| was not found in the list of launched countries.
    autofill_metrics::LogCardUploadEnabledMetric(
        autofill_metrics::CardUploadEnabled::kUnsupportedCountry,
        signin_state_for_metrics);
    LogCardUploadDisabled(log_manager, "UNSUPPORTED_COUNTRY");
    return false;
  }

  autofill_metrics::LogCardUploadEnabledMetric(
      autofill_metrics::CardUploadEnabled::kEnabledForCountry,
      signin_state_for_metrics);
  LogCardUploadEnabled(log_manager);
  return true;
}

bool IsInAutofillSuggestionsDisabledExperiment() {
  std::string group_name =
      base::FieldTrialList::FindFullName("AutofillEnabled");
  return group_name == "Disabled";
}

bool IsCreditCardFidoAuthenticationEnabled() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  // Better Auth project is fully launched on Windows/Mac for Desktop, and
  // Android for mobile.
  return true;
#else
  return false;
#endif
}

bool ShouldShowIbanOnSettingsPage(const std::string& user_country_code,
                                  PrefService* pref_service) {
  std::string country_code = base::ToUpperASCII(user_country_code);
  return Iban::IsIbanApplicableInCountry(user_country_code) ||
         prefs::HasSeenIban(pref_service);
}

bool IsDeviceAuthAvailable(
    device_reauth::DeviceAuthenticator* device_authenticator) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  CHECK(device_authenticator);
  return device_authenticator->CanAuthenticateWithBiometricOrScreenLock();
#else
  return false;
#endif
}
bool IsTouchToFillPaymentMethodSupported() {
#if BUILDFLAG(IS_ANDROID)
  // Touch To Fill is only supported on Android.
  return true;
#else
  return false;
#endif
}

bool IsUserOptedInWalletSyncTransport(const PrefService* prefs,
                                      const CoreAccountId& account_id) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // On mobile, no specific opt-in is required.
  return true;
#else
  if (prefs->GetBoolean(::prefs::kExplicitBrowserSignin)) {
    // Explicit browser signin makes the explicit opt-in unnecessary.
    return true;
  }

  // Get the hash of the account id.
  std::string account_hash =
      base::Base64Encode(crypto::SHA256HashString(account_id.ToString()));

  // Return whether the wallet opt-in bit is set.
  return GetSyncTransportOptInBitFieldForAccount(prefs, account_hash) &
         prefs::sync_transport_opt_in::kWallet;
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

void SetUserOptedInWalletSyncTransport(PrefService* prefs,
                                       const CoreAccountId& account_id,
                                       bool opted_in) {
  // Get the hash of the account id. The hashing here is only a secondary bit of
  // obfuscation. The primary privacy guarantees are handled by clearing this
  // whenever cookies are cleared.
  std::string account_hash =
      base::Base64Encode(crypto::SHA256HashString(account_id.ToString()));

  ScopedDictPrefUpdate update(prefs, prefs::kAutofillSyncTransportOptIn);
  int value = GetSyncTransportOptInBitFieldForAccount(prefs, account_hash);

  // If the user has opted in, set that bit while leaving the others intact.
  if (opted_in) {
    update->Set(account_hash, value | prefs::sync_transport_opt_in::kWallet);
    return;
  }

  // Invert the mask in order to reset the Wallet bit while leaving the other
  // bits intact, or remove the key entirely if the Wallet was the only opt-in.
  if (value & ~prefs::sync_transport_opt_in::kWallet) {
    update->Set(account_hash, value & ~prefs::sync_transport_opt_in::kWallet);
  } else {
    update->Remove(account_hash);
  }
}

}  // namespace autofill
