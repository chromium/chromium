// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXPERIMENTS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXPERIMENTS_H_

#include <string>

#include "build/build_config.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

class PrefService;

namespace syncer {
class SyncService;
}

namespace device_reauth {
class DeviceAuthenticator;
}

namespace autofill {

class LogManager;
class PersonalDataManager;

// Returns true if uploading credit cards to Wallet servers is enabled. This
// requires the appropriate flags and user settings to be true and the user to
// be a member of a supported domain.
bool IsCreditCardUploadEnabled(
    const syncer::SyncService* sync_service,
    const std::string& user_country,
    AutofillMetrics::PaymentsSigninState signin_state_for_metrics,
    LogManager* log_manager);

// Returns true if autofill local card migration flow is enabled.
bool IsCreditCardMigrationEnabled(PersonalDataManager* personal_data_manager,
                                  syncer::SyncService* sync_service,
                                  bool is_test_mode,
                                  LogManager* log_manager);

// Returns true if autofill suggestions are disabled via experiment. The
// disabled experiment isn't the same as disabling autofill completely since we
// still want to run detection code for metrics purposes. This experiment just
// disables providing suggestions.
bool IsInAutofillSuggestionsDisabledExperiment();

// Returns true if the feature is explicitly enabled by the corresponding Finch
// flag, or if launched in general for this platform, which is true for Windows,
// Android, and macOS.
bool IsCreditCardFidoAuthenticationEnabled();

// Returns true if IBAN is enabled and at least one of the two conditions below
// is meet:
// 1. the user's country is relevant to IBAN.
// 2. the user has submitted an IBAN form or added an IBAN via Chrome payment
//    settings page in the past.
bool ShouldShowIbanOnSettingsPage(const std::string& user_country_code,
                                  PrefService* pref_service);

// Returns true if we can use device authentication to authenticate the user.
// We currently only support biometric authentication for the same.
bool IsDeviceAuthAvailable(
    device_reauth::DeviceAuthenticator* device_authenticator);

// Returns true if the Touch To Fill feature is supported by platform.
bool IsTouchToFillPaymentMethodSupported();

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXPERIMENTS_H_
