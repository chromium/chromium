// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/multi_store_password_save_manager.h"

#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/common/gaia_id_hash.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/form_saver.h"
#include "components/password_manager/core/browser/form_saver_impl.h"
#include "components/password_manager/core/browser/password_feature_manager_impl.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"

namespace password_manager {

namespace {

std::vector<const PasswordForm*> MatchesInStore(
    const std::vector<const PasswordForm*>& matches,
    PasswordForm::Store store) {
  std::vector<const PasswordForm*> store_matches;
  for (const PasswordForm* match : matches) {
    DCHECK(match->in_store != PasswordForm::Store::kNotSet);
    if (match->in_store == store)
      store_matches.push_back(match);
  }
  return store_matches;
}

std::vector<const PasswordForm*> AccountStoreMatches(
    const std::vector<const PasswordForm*>& matches) {
  return MatchesInStore(matches, PasswordForm::Store::kAccountStore);
}

std::vector<const PasswordForm*> ProfileStoreMatches(
    const std::vector<const PasswordForm*>& matches) {
  return MatchesInStore(matches, PasswordForm::Store::kProfileStore);
}

bool AccountStoreMatchesContainForm(
    const std::vector<const PasswordForm*>& matches,
    const PasswordForm& form) {
  PasswordForm form_in_account_store(form);
  form_in_account_store.in_store = PasswordForm::Store::kAccountStore;
  for (const PasswordForm* match : matches) {
    if (form_in_account_store == *match)
      return true;
  }
  return false;
}

PendingCredentialsState ResolvePendingCredentialsStates(
    PendingCredentialsState profile_state,
    PendingCredentialsState account_state) {
  // The result of this resolution will be used to decide whether to show a
  // save or update prompt to the user. Resolve the two states to a single
  // "canonical" one according to the following hierarchy:
  // AUTOMATIC_SAVE > EQUAL_TO_SAVED_MATCH > UPDATE > NEW_LOGIN
  // Note that UPDATE or NEW_LOGIN will result in an Update or Save bubble to
  // be shown, while AUTOMATIC_SAVE and EQUAL_TO_SAVED_MATCH will result in a
  // silent save/update.
  // Some interesting cases:
  // NEW_LOGIN means that store doesn't know about the credential yet. If the
  // other store knows anything at all, then that always wins.
  // EQUAL_TO_SAVED_MATCH vs UPDATE: This means one store had a match, the other
  // had a mismatch (same username but different password). The mismatch should
  // be updated silently, so resolve to EQUAL so that there's no visible prompt.
  // AUTOMATIC_SAVE vs EQUAL_TO_SAVED_MATCH: These are both silent, so it
  // doesn't really matter to which one we resolve.
  // AUTOMATIC_SAVE vs UPDATE: Similar to EQUAL_TO_SAVED_MATCH vs UPDATE, the
  // mismatch should be silently updated.
  if (profile_state == PendingCredentialsState::AUTOMATIC_SAVE ||
      account_state == PendingCredentialsState::AUTOMATIC_SAVE) {
    return PendingCredentialsState::AUTOMATIC_SAVE;
  }
  if (profile_state == PendingCredentialsState::EQUAL_TO_SAVED_MATCH ||
      account_state == PendingCredentialsState::EQUAL_TO_SAVED_MATCH) {
    return PendingCredentialsState::EQUAL_TO_SAVED_MATCH;
  }
  if (profile_state == PendingCredentialsState::UPDATE ||
      account_state == PendingCredentialsState::UPDATE) {
    return PendingCredentialsState::UPDATE;
  }
  if (profile_state == PendingCredentialsState::NEW_LOGIN ||
      account_state == PendingCredentialsState::NEW_LOGIN) {
    return PendingCredentialsState::NEW_LOGIN;
  }
  NOTREACHED();
  return PendingCredentialsState::NONE;
}

// Returns a PasswordForm that has all fields taken from |update| except
// date_created, date_synced, times_used and moving_blocked_for_list that are
// taken from |original_form|.
PasswordForm UpdateFormPreservingDifferentFieldsAcrossStores(
    const PasswordForm& original_form,
    const PasswordForm& update) {
  PasswordForm result(update);
  result.date_created = original_form.date_created;
  result.date_synced = original_form.date_synced;
  result.times_used = original_form.times_used;
  result.moving_blocked_for_list = original_form.moving_blocked_for_list;
  return result;
}

}  // namespace

MultiStorePasswordSaveManager::MultiStorePasswordSaveManager(
    std::unique_ptr<FormSaver> profile_form_saver,
    std::unique_ptr<FormSaver> account_form_saver)
    : PasswordSaveManagerImpl(std::move(profile_form_saver)),
      account_store_form_saver_(std::move(account_form_saver)) {
  DCHECK(account_store_form_saver_);
}

MultiStorePasswordSaveManager::~MultiStorePasswordSaveManager() = default;

void MultiStorePasswordSaveManager::SavePendingToStoreImpl(
    const PasswordForm& parsed_submitted_form) {
  auto matches = form_fetcher_->GetAllRelevantMatches();
  PendingCredentialsStates states =
      ComputePendingCredentialsStates(parsed_submitted_form, matches);

  auto account_matches = AccountStoreMatches(matches);
  auto profile_matches = ProfileStoreMatches(matches);

  base::string16 old_account_password =
      states.similar_saved_form_from_account_store
          ? states.similar_saved_form_from_account_store->password_value
          : base::string16();
  base::string16 old_profile_password =
      states.similar_saved_form_from_profile_store
          ? states.similar_saved_form_from_profile_store->password_value
          : base::string16();

  if (states.profile_store_state == PendingCredentialsState::NEW_LOGIN &&
      states.account_store_state == PendingCredentialsState::NEW_LOGIN) {
    // If the credential is new to both stores, store it only in the default
    // store.
    if (AccountStoreIsDefault()) {
      // TODO(crbug.com/1012203): Record UMA for how many passwords get dropped
      // here. In rare cases it could happen that the user *was* opted in when
      // the save dialog was shown, but now isn't anymore.
      if (IsOptedInForAccountStorage()) {
        account_store_form_saver_->Save(pending_credentials_, account_matches,
                                        old_account_password);
      }
    } else {
      form_saver_->Save(pending_credentials_, profile_matches,
                        old_profile_password);
    }
    return;
  }

  switch (states.profile_store_state) {
    case PendingCredentialsState::AUTOMATIC_SAVE:
      form_saver_->Save(pending_credentials_, profile_matches,
                        old_profile_password);
      break;
    case PendingCredentialsState::UPDATE:
    case PendingCredentialsState::EQUAL_TO_SAVED_MATCH: {
      // If the submitted credentials exists in both stores,
      // |pending_credentials_| might be from the account store (and thus not
      // have a moving_blocked_for_list). We need to preserve any existing list.
      // Same applies for other fields. Check the comment on
      // UpdateFormPreservingDifferentFieldsAcrossStores().
      PasswordForm form_to_update =
          UpdateFormPreservingDifferentFieldsAcrossStores(
              *states.similar_saved_form_from_profile_store,
              pending_credentials_);
      // For other cases, |pending_credentials_.times_used| is updated in
      // UpdateMetadataForUsage() invoked from UploadVotesAndMetrics().
      // UpdateFormPreservingDifferentFieldsAcrossStores() preserved the
      // original times_used, and hence we should increment it here.
      form_to_update.times_used++;
      form_saver_->Update(form_to_update, profile_matches,
                          old_profile_password);
    } break;
    // The NEW_LOGIN case was already handled separately above.
    case PendingCredentialsState::NEW_LOGIN:
    case PendingCredentialsState::NONE:
      break;
  }

  // TODO(crbug.com/1012203): Record UMA for how many passwords get dropped
  // here. In rare cases it could happen that the user *was* opted in when
  // the save dialog was shown, but now isn't anymore.
  if (IsOptedInForAccountStorage()) {
    switch (states.account_store_state) {
      case PendingCredentialsState::AUTOMATIC_SAVE:
        account_store_form_saver_->Save(pending_credentials_, account_matches,
                                        old_account_password);
        break;
      case PendingCredentialsState::UPDATE:
      case PendingCredentialsState::EQUAL_TO_SAVED_MATCH: {
        // If the submitted credentials exists in both stores,
        // .|pending_credentials_| might be from the profile store (and thus
        // has a moving_blocked_for_list). We need to preserve any existing
        // values. Same applies for other fields. Check the comment on
        // UpdateFormPreservingDifferentFieldsAcrossStores().
        PasswordForm form_to_update =
            UpdateFormPreservingDifferentFieldsAcrossStores(
                *states.similar_saved_form_from_account_store,
                pending_credentials_);
        // For other cases, |pending_credentials_.times_used| is updated in
        // UpdateMetadataForUsage() invoked from UploadVotesAndMetrics().
        // UpdateFormPreservingDifferentFieldsAcrossStores() preserved the
        // original times_used, and hence we should increment it here.
        form_to_update.times_used++;
        account_store_form_saver_->Update(form_to_update, account_matches,
                                          old_account_password);
      } break;
      // The NEW_LOGIN case was already handled separately above.
      case PendingCredentialsState::NEW_LOGIN:
      case PendingCredentialsState::NONE:
        break;
    }
  }
}

void MultiStorePasswordSaveManager::PermanentlyBlacklist(
    const PasswordStore::FormDigest& form_digest) {
  DCHECK(!client_->IsIncognito());
  if (IsOptedInForAccountStorage() && AccountStoreIsDefault()) {
    account_store_form_saver_->PermanentlyBlacklist(form_digest);
  } else {
    // For users who aren't yet opted-in to the account storage, we store their
    // blacklisted entries in the profile store.
    form_saver_->PermanentlyBlacklist(form_digest);
  }
}

void MultiStorePasswordSaveManager::Unblacklist(
    const PasswordStore::FormDigest& form_digest) {
  // Try to unblacklist in both stores anyway because if credentials don't
  // exist, the unblacklist operation is no-op.
  form_saver_->Unblacklist(form_digest);
  if (IsOptedInForAccountStorage())
    account_store_form_saver_->Unblacklist(form_digest);
}

std::unique_ptr<PasswordSaveManager> MultiStorePasswordSaveManager::Clone() {
  auto result = std::make_unique<MultiStorePasswordSaveManager>(
      form_saver_->Clone(), account_store_form_saver_->Clone());
  CloneInto(result.get());
  return result;
}

void MultiStorePasswordSaveManager::MoveCredentialsToAccountStore(
    metrics_util::MoveToAccountStoreTrigger trigger) {
  base::UmaHistogramEnumeration(
      "PasswordManager.AccountStorage.MoveToAccountStoreFlowAccepted", trigger);

  // TODO(crbug.com/1032992): Moving credentials upon an update. FormFetch will
  // have an outdated credentials. Fix it if this turns out to be a product
  // requirement.

  std::vector<const PasswordForm*> account_store_matches =
      AccountStoreMatches(form_fetcher_->GetNonFederatedMatches());
  const std::vector<const PasswordForm*> account_store_federated_matches =
      AccountStoreMatches(form_fetcher_->GetFederatedMatches());
  account_store_matches.insert(account_store_matches.end(),
                               account_store_federated_matches.begin(),
                               account_store_federated_matches.end());

  std::vector<const PasswordForm*> profile_store_matches =
      ProfileStoreMatches(form_fetcher_->GetNonFederatedMatches());
  const std::vector<const PasswordForm*> profile_store_federated_matches =
      ProfileStoreMatches(form_fetcher_->GetFederatedMatches());
  profile_store_matches.insert(profile_store_matches.end(),
                               profile_store_federated_matches.begin(),
                               profile_store_federated_matches.end());

  for (const PasswordForm* match : profile_store_matches) {
    DCHECK(!match->IsUsingAccountStore());
    // Ignore credentials matches for other usernames.
    if (match->username_value != pending_credentials_.username_value)
      continue;

    // Don't call Save() if the credential already exists in the account
    // store, 1) to avoid unnecessary sync cycles, 2) to avoid potential
    // last_used_date update.
    if (!AccountStoreMatchesContainForm(account_store_matches, *match)) {
      PasswordForm match_copy = *match;
      match_copy.moving_blocked_for_list.clear();
      account_store_form_saver_->Save(match_copy, account_store_matches,
                                      /*old_password=*/base::string16());
    }
    form_saver_->Remove(*match);
  }
}

void MultiStorePasswordSaveManager::BlockMovingToAccountStoreFor(
    const autofill::GaiaIdHash& gaia_id_hash) {
  // TODO(crbug.com/1032992): This doesn't work if moving is offered upon update
  // prompts.

  // We offer moving credentials to the account store only upon successful
  // login. This entails that the credentials must exist in the profile store.
  PendingCredentialsStates states = ComputePendingCredentialsStates(
      pending_credentials_, form_fetcher_->GetAllRelevantMatches());
  DCHECK(states.similar_saved_form_from_profile_store);
  DCHECK_EQ(PendingCredentialsState::EQUAL_TO_SAVED_MATCH,
            states.profile_store_state);

  // If the submitted credentials exists in both stores, .|pending_credentials_|
  // might be from the account store (and thus not have a
  // moving_blocked_for_list). We need to preserve any existing list, so
  // explicitly copy it over from the profile store match.
  PasswordForm form_to_block(pending_credentials_);
  form_to_block.moving_blocked_for_list =
      states.similar_saved_form_from_profile_store->moving_blocked_for_list;
  form_to_block.moving_blocked_for_list.push_back(gaia_id_hash);

  // No need to pass matches to Update(). It's only used for post processing
  // (e.g. updating the password for other credentials with the same
  // old password).
  form_saver_->Update(form_to_block, /*matches=*/{},
                      form_to_block.password_value);
}

std::pair<const PasswordForm*, PendingCredentialsState>
MultiStorePasswordSaveManager::FindSimilarSavedFormAndComputeState(
    const PasswordForm& parsed_submitted_form) const {
  PendingCredentialsStates states = ComputePendingCredentialsStates(
      parsed_submitted_form, form_fetcher_->GetBestMatches());

  // Resolve the two states to a single canonical one. This will be used to
  // decide what UI bubble (if any) to show to the user.
  PendingCredentialsState resolved_state = ResolvePendingCredentialsStates(
      states.profile_store_state, states.account_store_state);

  // Choose which of the saved forms (if any) to use as the base for updating,
  // based on which of the two states won the resolution.
  // Note that if we got the same state for both stores, then it doesn't really
  // matter which one we pick for updating, since the result will be the same
  // anyway.
  const PasswordForm* resolved_similar_saved_form = nullptr;
  if (resolved_state == states.profile_store_state)
    resolved_similar_saved_form = states.similar_saved_form_from_profile_store;
  else if (resolved_state == states.account_store_state)
    resolved_similar_saved_form = states.similar_saved_form_from_account_store;

  return std::make_pair(resolved_similar_saved_form, resolved_state);
}

FormSaver* MultiStorePasswordSaveManager::GetFormSaverForGeneration() {
  return IsOptedInForAccountStorage() ? account_store_form_saver_.get()
                                      : form_saver_.get();
}

std::vector<const PasswordForm*>
MultiStorePasswordSaveManager::GetRelevantMatchesForGeneration(
    const std::vector<const PasswordForm*>& matches) {
  //  For account store users, only matches in the account store should be
  //  considered for conflict resolution during generation.
  return IsOptedInForAccountStorage()
             ? MatchesInStore(matches, PasswordForm::Store::kAccountStore)
             : matches;
}

// static
MultiStorePasswordSaveManager::PendingCredentialsStates
MultiStorePasswordSaveManager::ComputePendingCredentialsStates(
    const PasswordForm& parsed_submitted_form,
    const std::vector<const PasswordForm*>& matches) {
  PendingCredentialsStates result;

  // Try to find a similar existing saved form from each of the stores.
  result.similar_saved_form_from_profile_store =
      password_manager_util::GetMatchForUpdating(parsed_submitted_form,
                                                 ProfileStoreMatches(matches));
  result.similar_saved_form_from_account_store =
      password_manager_util::GetMatchForUpdating(parsed_submitted_form,
                                                 AccountStoreMatches(matches));

  // Compute the PendingCredentialsState (i.e. what to do - save, update, silent
  // update) separately for the two stores.
  result.profile_store_state = ComputePendingCredentialsState(
      parsed_submitted_form, result.similar_saved_form_from_profile_store);
  result.account_store_state = ComputePendingCredentialsState(
      parsed_submitted_form, result.similar_saved_form_from_account_store);

  return result;
}

bool MultiStorePasswordSaveManager::IsOptedInForAccountStorage() const {
  return client_->GetPasswordFeatureManager()->IsOptedInForAccountStorage();
}

bool MultiStorePasswordSaveManager::AccountStoreIsDefault() const {
  return client_->GetPasswordFeatureManager()->GetDefaultPasswordStore() ==
         PasswordForm::Store::kAccountStore;
}

}  // namespace password_manager
