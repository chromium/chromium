// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_experiments.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/variations/variations_associated_data.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace autofill {
namespace {
void LogCardUploadDisabled(LogManager* log_manager, base::StringPiece context) {
  LOG_AF(log_manager) << LoggingScope::kCreditCardUploadStatus
                      << LogMessage::kCreditCardUploadDisabled << context
                      << CTag{};
}

void LogCardUploadEnabled(LogManager* log_manager) {
  LOG_AF(log_manager) << LoggingScope::kCreditCardUploadStatus
                      << LogMessage::kCreditCardUploadEnabled << CTag{};
}

// Given an email account domain, returns the contents before the first dot.
std::string GetFirstSegmentFromDomain(const std::string& domain) {
  size_t separator_pos = domain.find('.');
  if (separator_pos != domain.npos)
    return domain.substr(0, separator_pos);

  NOTREACHED() << "'.' not found in email domain: " << domain;
  return std::string();
}
}  // namespace

// The list of countries for which the credit card upload save feature is fully
// launched. Last updated M118.
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
    "QA", "RE", "RO", "RU", "SB", "SC", "SE", "SG", "SI", "SJ", "SK", "SL",
    "SM", "SN", "SR", "ST", "SV", "SZ", "TC", "TD", "TG", "TH", "TL", "TM",
    "TO", "TR", "TT", "TV", "TW", "TZ", "UA", "UG", "US", "UY", "VC", "VE",
    "VG", "VI", "VN", "VU", "WS", "YT", "ZA", "ZM", "ZW"};

// The list of supported additional email domains for credit card upload if the
// AutofillUpstreamAllowAdditionalEmailDomains flag is enabled. Specifically
// contains only the first part of the domain, so example.com, example.co.uk,
// example.fr, etc., are all allowed for "example".
const char* const kSupportedAdditionalDomains[] = {"aol",
                                                   "att",
                                                   "btinternet",
                                                   "comcast",
                                                   "gmx",
                                                   "hotmail",
                                                   "icloud",
                                                   /*libero.it*/ "libero",
                                                   "live",
                                                   "me",
                                                   "msn",
                                                   /*orange.fr*/ "orange",
                                                   "outlook",
                                                   "sbcglobal",
                                                   /*seznam.cz*/ "seznam",
                                                   "sky",
                                                   "verizon",
                                                   /*wp.pl*/ "wp",
                                                   "yahoo",
                                                   "ymail"};

bool IsCreditCardUploadEnabled(
    const syncer::SyncService* sync_service,
    const std::string& user_email,
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
    return false;
  }

  // With `AutofillDecoupleAddressPaymentSyncSettings`, the address and payment
  // sync settings become independent. However, since address information is
  // uploaded during the server card saving flow, credit card upload is not
  // available when address sync is disabled.
  // TODO(crbug.com/1462552): Simplify once IsSyncFeatureActive() is deleted
  // from the codebase.
  if (sync_service->IsSyncFeatureActive() ||
      base::FeatureList::IsEnabled(
          syncer::kSyncDecoupleAddressPaymentSettings)) {
    if (!sync_service->GetActiveDataTypes().Has(syncer::AUTOFILL_PROFILE)) {
      // In full sync mode, we only allow card upload when addresses are also
      // active, because we upload potential billing addresses with the card.
      autofill_metrics::LogCardUploadEnabledMetric(
          autofill_metrics::CardUploadEnabled::
              kSyncServiceMissingAutofillProfileActiveType,
          signin_state_for_metrics);
      LogCardUploadDisabled(
          log_manager,
          "SYNC_SERVICE_MISSING_AUTOFILL_PROFILE_ACTIVE_DATA_TYPE");
      return false;
    }
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

  // Check that the user's account email address is known.
  if (user_email.empty()) {
    autofill_metrics::LogCardUploadEnabledMetric(
        autofill_metrics::CardUploadEnabled::kEmailEmpty,
        signin_state_for_metrics);
    LogCardUploadDisabled(log_manager, "USER_EMAIL_EMPTY");
    return false;
  }

  // Check that the user is logged into a supported domain.
  std::string domain = gaia::ExtractDomainName(user_email);
  std::string domain_first_segment = GetFirstSegmentFromDomain(domain);
  // If the flag to allow all email domains is enabled, any domain is accepted.
  bool all_domains_supported = base::FeatureList::IsEnabled(
      features::kAutofillUpstreamAllowAllEmailDomains);
  // If the flag to allow select email domains is enabled, domains from popular
  // account providers are accepted.
  bool using_supported_additional_domain =
      base::FeatureList::IsEnabled(
          features::kAutofillUpstreamAllowAdditionalEmailDomains) &&
      base::Contains(kSupportedAdditionalDomains, domain_first_segment);
  // Otherwise, restrict credit card upload only to Google Accounts with
  // @googlemail, @gmail, @google, or @chromium domains.
  // example.com is on the list because ChromeOS tests rely on using this. That
  // should be fine, since example.com is an IANA reserved domain.
  bool using_google_domain = domain == "googlemail.com" ||
                             domain == "gmail.com" || domain == "google.com" ||
                             domain == "chromium.org" ||
                             domain == "example.com";
  if (!all_domains_supported && !using_supported_additional_domain &&
      !using_google_domain) {
    autofill_metrics::LogCardUploadEnabledMetric(
        autofill_metrics::CardUploadEnabled::kEmailDomainNotSupported,
        signin_state_for_metrics);
    LogCardUploadDisabled(log_manager, "USER_EMAIL_DOMAIN_NOT_SUPPORTED");
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
      base::ranges::find(kAutofillUpstreamLaunchedCountries, country_code);
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

bool IsCreditCardMigrationEnabled(PersonalDataManager* personal_data_manager,
                                  syncer::SyncService* sync_service,
                                  bool is_test_mode,
                                  LogManager* log_manager) {
  // If |is_test_mode| is set, assume we are in a browsertest and
  // credit card upload should be enabled by default to fix flaky
  // local card migration browsertests.
  if (!is_test_mode &&
      !IsCreditCardUploadEnabled(
          sync_service,
          personal_data_manager->GetAccountInfoForPaymentsServer().email,
          personal_data_manager->GetCountryCodeForExperimentGroup(),
          personal_data_manager->GetPaymentsSigninStateForMetrics(),
          log_manager)) {
    return false;
  }

  if (!payments::HasGooglePaymentsAccount(personal_data_manager))
    return false;

  return personal_data_manager->IsPaymentsDownloadActive();
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
  return device_authenticator->CanAuthenticateWithBiometricOrScreenLock() &&
         base::FeatureList::IsEnabled(
             features::kAutofillEnablePaymentsMandatoryReauth);
#else
  return false;
#endif
}

}  // namespace autofill
