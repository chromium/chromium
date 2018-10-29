// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_util.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/blacklisted_duplicates_cleaner.h"
#include "components/password_manager/core/browser/credentials_cleaner.h"
#include "components/password_manager/core/browser/credentials_cleaner_runner.h"
#include "components/password_manager/core/browser/http_credentials_cleaner.h"
#include "components/password_manager/core/browser/invalid_realm_credential_cleaner.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"

using autofill::PasswordForm;

namespace password_manager_util {
namespace {

// Return true if
// 1.|lhs| is non-PSL match, |rhs| is PSL match or
// 2.|lhs| and |rhs| have the same value of |is_public_suffix_match|, and |lhs|
// is preferred while |rhs| is not preferred.
bool IsBetterMatch(const PasswordForm* lhs, const PasswordForm* rhs) {
  return std::make_pair(!lhs->is_public_suffix_match, lhs->preferred) >
         std::make_pair(!rhs->is_public_suffix_match, rhs->preferred);
}

}  // namespace

// Update |credential| to reflect usage.
void UpdateMetadataForUsage(PasswordForm* credential) {
  ++credential->times_used;

  // Remove alternate usernames. At this point we assume that we have found
  // the right username.
  credential->other_possible_usernames.clear();
}

password_manager::SyncState GetPasswordSyncState(
    const syncer::SyncService* sync_service) {
  if (sync_service && sync_service->IsFirstSetupComplete() &&
      sync_service->IsSyncFeatureActive() &&
      sync_service->GetActiveDataTypes().Has(syncer::PASSWORDS)) {
    return sync_service->IsUsingSecondaryPassphrase()
               ? password_manager::SYNCING_WITH_CUSTOM_PASSPHRASE
               : password_manager::SYNCING_NORMAL_ENCRYPTION;
  }
  return password_manager::NOT_SYNCING;
}

void FindDuplicates(std::vector<std::unique_ptr<PasswordForm>>* forms,
                    std::vector<std::unique_ptr<PasswordForm>>* duplicates,
                    std::vector<std::vector<PasswordForm*>>* tag_groups) {
  if (forms->empty())
    return;

  // Linux backends used to treat the first form as a prime oneamong the
  // duplicates. Therefore, the caller should try to preserve it.
  std::stable_sort(forms->begin(), forms->end(), autofill::LessThanUniqueKey());

  std::vector<std::unique_ptr<PasswordForm>> unique_forms;
  unique_forms.push_back(std::move(forms->front()));
  if (tag_groups) {
    tag_groups->clear();
    tag_groups->push_back(std::vector<PasswordForm*>());
    tag_groups->front().push_back(unique_forms.front().get());
  }
  for (auto it = forms->begin() + 1; it != forms->end(); ++it) {
    if (ArePasswordFormUniqueKeyEqual(**it, *unique_forms.back())) {
      if (tag_groups)
        tag_groups->back().push_back(it->get());
      duplicates->push_back(std::move(*it));
    } else {
      if (tag_groups)
        tag_groups->push_back(std::vector<PasswordForm*>(1, it->get()));
      unique_forms.push_back(std::move(*it));
    }
  }
  forms->swap(unique_forms);
}

void TrimUsernameOnlyCredentials(
    std::vector<std::unique_ptr<PasswordForm>>* android_credentials) {
  // Remove username-only credentials which are not federated.
  base::EraseIf(*android_credentials,
                [](const std::unique_ptr<PasswordForm>& form) {
                  return form->scheme == PasswordForm::SCHEME_USERNAME_ONLY &&
                         form->federation_origin.opaque();
                });

  // Set "skip_zero_click" on federated credentials.
  std::for_each(android_credentials->begin(), android_credentials->end(),
                [](const std::unique_ptr<PasswordForm>& form) {
                  if (form->scheme == PasswordForm::SCHEME_USERNAME_ONLY)
                    form->skip_zero_click = true;
                });
}

bool IsLoggingActive(const password_manager::PasswordManagerClient* client) {
  const password_manager::LogManager* log_manager = client->GetLogManager();
  return log_manager && log_manager->IsLoggingActive();
}

bool ManualPasswordGenerationEnabled(
    password_manager::PasswordManagerDriver* driver) {
  password_manager::PasswordGenerationManager* password_generation_manager =
      driver ? driver->GetPasswordGenerationManager() : nullptr;
  if (!password_generation_manager ||
      !password_generation_manager->IsGenerationEnabled(false /*logging*/)) {
    return false;
  }

  LogPasswordGenerationEvent(
      autofill::password_generation::PASSWORD_GENERATION_CONTEXT_MENU_SHOWN);
  return true;
}

bool ShowAllSavedPasswordsContextMenuEnabled(
    password_manager::PasswordManagerDriver* driver) {
  password_manager::PasswordManager* password_manager =
      driver ? driver->GetPasswordManager() : nullptr;
  if (!password_manager)
    return false;

  password_manager::PasswordManagerClient* client = password_manager->client();
  if (!client || !client->IsFillingFallbackEnabledForCurrentPage())
    return false;

  LogContextOfShowAllSavedPasswordsShown(
      password_manager::metrics_util::
          SHOW_ALL_SAVED_PASSWORDS_CONTEXT_CONTEXT_MENU);

  return true;
}

void UserTriggeredShowAllSavedPasswordsFromContextMenu(
    autofill::AutofillClient* autofill_client) {
  if (!autofill_client)
    return;
  autofill_client->ExecuteCommand(
      autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY);
  password_manager::metrics_util::LogContextOfShowAllSavedPasswordsAccepted(
      password_manager::metrics_util::
          SHOW_ALL_SAVED_PASSWORDS_CONTEXT_CONTEXT_MENU);
}

void UserTriggeredManualGenerationFromContextMenu(
    password_manager::PasswordManagerClient* password_manager_client) {
  password_manager_client->GeneratePassword();
  LogPasswordGenerationEvent(
      autofill::password_generation::PASSWORD_GENERATION_CONTEXT_MENU_PRESSED);
}

// TODO(http://crbug.com/890318): Add unitests to check cleaners are correctly
// created.
void RemoveUselessCredentials(
    scoped_refptr<password_manager::PasswordStore> store,
    PrefService* prefs,
    int delay_in_seconds,
    base::RepeatingCallback<network::mojom::NetworkContext*()>
        network_context_getter) {
  // TODO(https://crbug.com/887889): Remove the knowledge of the particular
  // preferences from this code.

  const bool need_to_remove_blacklisted_duplicates = !prefs->GetBoolean(
      password_manager::prefs::kDuplicatedBlacklistedCredentialsRemoved);
  base::UmaHistogramBoolean(
      "PasswordManager.BlacklistedSites.NeedRemoveBlacklistDuplicates",
      need_to_remove_blacklisted_duplicates);

  const bool need_to_remove_invalid_credentials = !prefs->GetBoolean(
      password_manager::prefs::kCredentialsWithWrongSignonRealmRemoved);
  base::UmaHistogramBoolean(
      "PasswordManager.InvalidtHttpsCredentialsNeedToBeCleared",
      need_to_remove_invalid_credentials);

  // The object will delete itself once the clearing tasks are done.
  auto* cleaning_tasks_runner =
      new password_manager::CredentialsCleanerRunner();

  // Cleaning of credentials with invalid signon_realm needs to search for
  // blacklisted non-HTML HTTPS credentials for a corresponding HTTP
  // credentials. Thus, this clean-up must be done before cleaning blacklisted
  // credentials. Otherwise finding a corresponding HTTP credentials will fail.
  if (need_to_remove_invalid_credentials) {
    cleaning_tasks_runner->AddCleaningTask(
        std::make_unique<password_manager::InvalidRealmCredentialCleaner>(
            store, prefs));
  }

  if (need_to_remove_blacklisted_duplicates) {
    cleaning_tasks_runner->AddCleaningTask(
        std::make_unique<password_manager::BlacklistedDuplicatesCleaner>(
            store, prefs));
  }

#if !defined(OS_IOS)
  // Can be null for some unittests.
  if (!network_context_getter.is_null() &&
      password_manager::HttpCredentialCleaner::ShouldRunCleanUp(prefs)) {
    cleaning_tasks_runner->AddCleaningTask(
        std::make_unique<password_manager::HttpCredentialCleaner>(
            store, network_context_getter, prefs));
  }
#endif  // !defined(OS_IOS)

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&password_manager::CredentialsCleanerRunner::StartCleaning,
                     base::Unretained(cleaning_tasks_runner)),
      base::TimeDelta::FromSeconds(delay_in_seconds));
}

base::StringPiece GetSignonRealmWithProtocolExcluded(const PasswordForm& form) {
  base::StringPiece signon_realm_protocol_excluded = form.signon_realm;

  // Find the web origin (with protocol excluded) in the signon_realm.
  const size_t after_protocol =
      signon_realm_protocol_excluded.find(form.origin.GetOrigin().GetContent());
  DCHECK_NE(after_protocol, base::StringPiece::npos);

  // Keep the string starting with position |after_protocol|.
  signon_realm_protocol_excluded =
      signon_realm_protocol_excluded.substr(after_protocol);
  return signon_realm_protocol_excluded;
}

void FindBestMatches(
    std::vector<const PasswordForm*> matches,
    std::map<base::string16, const PasswordForm*>* best_matches,
    std::vector<const PasswordForm*>* not_best_matches,
    const PasswordForm** preferred_match) {
  DCHECK(std::all_of(
      matches.begin(), matches.end(),
      [](const PasswordForm* match) { return !match->blacklisted_by_user; }));
  DCHECK(best_matches);
  DCHECK(not_best_matches);
  DCHECK(preferred_match);

  *preferred_match = nullptr;
  best_matches->clear();
  not_best_matches->clear();

  if (matches.empty())
    return;

  // Sort matches using IsBetterMatch predicate.
  std::sort(matches.begin(), matches.end(), IsBetterMatch);
  for (const auto* match : matches) {
    const base::string16& username = match->username_value;
    // The first match for |username| in the sorted array is best match.
    if (best_matches->find(username) == best_matches->end())
      best_matches->insert(std::make_pair(username, match));
    else
      not_best_matches->push_back(match);
  }

  *preferred_match = *matches.begin();
}

}  // namespace password_manager_util
