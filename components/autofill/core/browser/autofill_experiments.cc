// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_experiments.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/variations/variations_associated_data.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

namespace autofill {

bool IsCreditCardUploadEnabled(const PrefService* pref_service,
                               const syncer::SyncService* sync_service,
                               const std::string& user_email,
                               const AutofillSyncSigninState sync_state,
                               LogManager* log_manager) {
  if (!sync_service) {
    // If credit card sync is not active, we're not offering to upload cards.
    AutofillMetrics::LogCardUploadEnabledMetric(
        AutofillMetrics::CardUploadEnabledMetric::SYNC_SERVICE_NULL,
        sync_state);
    if (log_manager)
      log_manager->Log() << LoggingScope::kContext << "SYNC_SERVICE_NULL";
    return false;
  }

  if (sync_service->GetAuthError().IsPersistentError()) {
    AutofillMetrics::LogCardUploadEnabledMetric(
        AutofillMetrics::CardUploadEnabledMetric::
            SYNC_SERVICE_PERSISTENT_AUTH_ERROR,
        sync_state);
    if (log_manager) {
      log_manager->Log() << LoggingScope::kContext
                         << "SYNC_SERVICE_PERSISTENT_ERROR";
    }
    return false;
  }

  if (!sync_service->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA)) {
    AutofillMetrics::LogCardUploadEnabledMetric(
        AutofillMetrics::CardUploadEnabledMetric::
            SYNC_SERVICE_MISSING_AUTOFILL_WALLET_DATA_ACTIVE_TYPE,
        sync_state);
    if (log_manager) {
      log_manager->Log()
          << LoggingScope::kContext
          << "SYNC_SERVICE_MISSING_AUTOFILL_WALLET_ACTIVE_DATA_TYPE";
    }
    return false;
  }

  if (sync_service->IsSyncFeatureActive()) {
    if (!sync_service->GetActiveDataTypes().Has(syncer::AUTOFILL_PROFILE)) {
      // In full sync mode, we only allow card upload when addresses are also
      // active, because we upload potential billing addresses with the card.
      AutofillMetrics::LogCardUploadEnabledMetric(
          AutofillMetrics::CardUploadEnabledMetric::
              SYNC_SERVICE_MISSING_AUTOFILL_PROFILE_ACTIVE_TYPE,
          sync_state);
      if (log_manager) {
        log_manager->Log()
            << LoggingScope::kContext
            << "SYNC_SERVICE_MISSING_AUTOFILL_PROFILE_ACTIVE_DATA_TYPE";
      }
      return false;
    }
  } else {
    // If Wallet sync is running even when sync the feature is off, the account
    // Wallet feature must be on.
    DCHECK(base::FeatureList::IsEnabled(
        features::kAutofillEnableAccountWalletStorage));
  }

  // Also don't offer upload for users that have a secondary sync passphrase.
  // Users who have enabled a passphrase have chosen to not make their sync
  // information accessible to Google. Since upload makes credit card data
  // available to other Google systems, disable it for passphrase users.
  if (sync_service->GetUserSettings()->IsUsingSecondaryPassphrase()) {
    AutofillMetrics::LogCardUploadEnabledMetric(
        AutofillMetrics::CardUploadEnabledMetric::
            USING_SECONDARY_SYNC_PASSPHRASE,
        sync_state);
    if (log_manager) {
      log_manager->Log() << LoggingScope::kContext
                         << "USER_HAS_SECONDARY_SYNC_PASSPHRASE";
    }
    return false;
  }

  // Don't offer upload for users that are only syncing locally, since they
  // won't receive the cards back from Google Payments.
  if (sync_service->IsLocalSyncEnabled()) {
    AutofillMetrics::LogCardUploadEnabledMetric(
        AutofillMetrics::CardUploadEnabledMetric::LOCAL_SYNC_ENABLED,
        sync_state);
    if (log_manager) {
      log_manager->Log() << LoggingScope::kContext
                         << "USER_ONLY_SYNCING_LOCALLY";
    }
    return false;
  }

  // Check Payments integration user setting.
  if (!prefs::IsPaymentsIntegrationEnabled(pref_service)) {
    AutofillMetrics::LogCardUploadEnabledMetric(
        AutofillMetrics::CardUploadEnabledMetric::PAYMENTS_INTEGRATION_DISABLED,
        sync_state);
    if (log_manager) {
      log_manager->Log() << LoggingScope::kContext
                         << "PAYMENTS_INTEGRATION_DISABLED";
    }
    return false;
  }

  // Check that the user is logged into a supported domain.
  if (user_email.empty()) {
    AutofillMetrics::LogCardUploadEnabledMetric(
        AutofillMetrics::CardUploadEnabledMetric::EMAIL_EMPTY, sync_state);
    if (log_manager)
      log_manager->Log() << LoggingScope::kContext << "USER_EMAIL_EMPTY";
    return false;
  }

  std::string domain = gaia::ExtractDomainName(user_email);
  // If the "allow all email domains" flag is off, restrict credit card upload
  // only to Google Accounts with @googlemail, @gmail, @google, or @chromium
  // domains.
  // example.com is on the list because ChromeOS tests rely on using this. That
  // should be fine, since example.com is an IANA reserved domain.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillUpstreamAllowAllEmailDomains) &&
      !(domain == "googlemail.com" || domain == "gmail.com" ||
        domain == "google.com" || domain == "chromium.org" ||
        domain == "example.com")) {
    AutofillMetrics::LogCardUploadEnabledMetric(
        AutofillMetrics::CardUploadEnabledMetric::EMAIL_DOMAIN_NOT_SUPPORTED,
        sync_state);
    if (log_manager) {
      log_manager->Log() << LoggingScope::kContext
                         << "USER_EMAIL_DOMAIN_NOT_SUPPORTED";
    }
    return false;
  }

  if (!base::FeatureList::IsEnabled(features::kAutofillUpstream)) {
    AutofillMetrics::LogCardUploadEnabledMetric(
        AutofillMetrics::CardUploadEnabledMetric::AUTOFILL_UPSTREAM_DISABLED,
        sync_state);
    if (log_manager) {
      log_manager->Log() << LoggingScope::kContext
                         << "AUTOFILL_UPSTREAM_NOT_ENABLED";
    }
    return false;
  }

  AutofillMetrics::LogCardUploadEnabledMetric(
      AutofillMetrics::CardUploadEnabledMetric::CARD_UPLOAD_ENABLED,
      sync_state);
  return true;
}

bool IsCreditCardMigrationEnabled(PersonalDataManager* personal_data_manager,
                                  PrefService* pref_service,
                                  syncer::SyncService* sync_service,
                                  bool is_test_mode,
                                  LogManager* log_manager) {
  // If |is_test_mode| is set, assume we are in a browsertest and
  // credit card upload should be enabled by default to fix flaky
  // local card migration browsertests.
  if (!is_test_mode &&
      !IsCreditCardUploadEnabled(
          pref_service, sync_service,
          personal_data_manager->GetAccountInfoForPaymentsServer().email,
          personal_data_manager->GetSyncSigninState(), log_manager)) {
    return false;
  }

  if (!autofill::payments::HasGooglePaymentsAccount(personal_data_manager))
    return false;

  switch (personal_data_manager->GetSyncSigninState()) {
    case AutofillSyncSigninState::kSignedOut:
    case AutofillSyncSigninState::kSignedIn:
    case AutofillSyncSigninState::kSyncPaused:
      return false;
    case AutofillSyncSigninState::kSignedInAndWalletSyncTransportEnabled:
      return base::FeatureList::IsEnabled(
          features::kAutofillEnableLocalCardMigrationForNonSyncUser);
    case AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled:
      return true;
    case AutofillSyncSigninState::kNumSyncStates:
      break;
  }
  NOTREACHED();
  return false;
}

bool IsInAutofillSuggestionsDisabledExperiment() {
  std::string group_name =
      base::FieldTrialList::FindFullName("AutofillEnabled");
  return group_name == "Disabled";
}

bool IsAutofillNoLocalSaveOnUploadSuccessExperimentEnabled() {
  return base::FeatureList::IsEnabled(
      features::kAutofillNoLocalSaveOnUploadSuccess);
}

bool OfferStoreUnmaskedCards(bool is_off_the_record) {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // The checkbox can be forced on with a flag, but by default we don't store
  // on Linux due to lack of system keychain integration. See crbug.com/162735
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableOfferStoreUnmaskedWalletCards);
#else
  // Never offer to store unmasked cards when off the record.
  if (is_off_the_record) {
    return false;
  }

  // Query the field trial before checking command line flags to ensure UMA
  // reports the correct group.
  std::string group_name =
      base::FieldTrialList::FindFullName("OfferStoreUnmaskedWalletCards");

  // The checkbox can be forced on or off with flags.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableOfferStoreUnmaskedWalletCards))
    return true;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableOfferStoreUnmaskedWalletCards))
    return false;

  // Otherwise use the field trial to show the checkbox or not.
  return group_name != "Disabled";
#endif
}

bool ShouldUseActiveSignedInAccount() {
  // If butter is enabled or the feature to get the Payment Identity from Sync
  // is enabled, the account of the active signed-in user should be used.
  return base::FeatureList::IsEnabled(
             features::kAutofillEnableAccountWalletStorage) ||
         base::FeatureList::IsEnabled(
             features::kAutofillGetPaymentsIdentityFromSync);
}

}  // namespace autofill
