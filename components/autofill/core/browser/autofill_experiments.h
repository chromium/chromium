// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXPERIMENTS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXPERIMENTS_H_

#include <string>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/sync_utils.h"

class PrefService;

namespace syncer {
class SyncService;
}

namespace autofill {

class LogManager;
class PersonalDataManager;

// Returns true if uploading credit cards to Wallet servers is enabled. This
// requires the appropriate flags and user settings to be true and the user to
// be a member of a supported domain.
bool IsCreditCardUploadEnabled(const PrefService* pref_service,
                               const syncer::SyncService* sync_service,
                               const std::string& user_email,
                               const AutofillSyncSigninState sync_state,
                               LogManager* log_manager);

// Returns true if autofill local card migration flow is enabled.
bool IsCreditCardMigrationEnabled(PersonalDataManager* personal_data_manager,
                                  PrefService* pref_service,
                                  syncer::SyncService* sync_service,
                                  bool is_test_mode,
                                  LogManager* log_manager);

// Returns true if autofill suggestions are disabled via experiment. The
// disabled experiment isn't the same as disabling autofill completely since we
// still want to run detection code for metrics purposes. This experiment just
// disables providing suggestions.
bool IsInAutofillSuggestionsDisabledExperiment();

// Returns whether locally saving card when credit card upload succeeds should
// be disabled.
bool IsAutofillNoLocalSaveOnUploadSuccessExperimentEnabled();

// Returns true if the user should be offered to locally store unmasked cards.
// This controls whether the option is presented at all rather than the default
// response of the option.
bool OfferStoreUnmaskedCards(bool is_off_the_record);

// Returns whether the account of the active signed-in user should be used.
bool ShouldUseActiveSignedInAccount();

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXPERIMENTS_H_
