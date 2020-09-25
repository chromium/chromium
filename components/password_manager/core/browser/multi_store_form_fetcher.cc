// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/multi_store_form_fetcher.h"

#include "base/check_op.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/statistics_table.h"

using Logger = autofill::SavePasswordProgressLogger;

namespace password_manager {

MultiStoreFormFetcher::MultiStoreFormFetcher(
    PasswordStore::FormDigest form_digest,
    const PasswordManagerClient* client,
    bool should_migrate_http_passwords)
    : FormFetcherImpl(form_digest, client, should_migrate_http_passwords) {}

MultiStoreFormFetcher::~MultiStoreFormFetcher() = default;

void MultiStoreFormFetcher::Fetch() {
  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogMessage(Logger::STRING_FETCH_METHOD);
    logger.LogNumber(Logger::STRING_FORM_FETCHER_STATE,
                     static_cast<int>(state_));
  }
  if (state_ == State::WAITING) {
    // There is currently a password store query in progress, need to re-fetch
    // store results later.
    need_to_refetch_ = true;
    return;
  }

  PasswordStore* account_password_store = client_->GetAccountPasswordStore();

  // Issue a fetch from the profile store and, if it exists, also from the
  // account store.
  // Set up |wait_counter_| *before* triggering any of the fetches. This ensures
  // that things work correctly (i.e. we don't notify of completion too early)
  // even if the fetches return synchronously (which is the case in tests).
  wait_counter_++;
  if (account_password_store)
    wait_counter_++;

  // Let the base class handle the fetch from the profile store.
  FormFetcherImpl::Fetch();
  if (account_password_store) {
    state_ = State::WAITING;
    account_password_store->GetLogins(form_digest_, this);
#if !defined(OS_IOS) && !defined(OS_ANDROID)
    // The desktop bubble needs this information.
    account_password_store->GetMatchingCompromisedCredentials(
        form_digest_.signon_realm, this);
#endif
  }
}

bool MultiStoreFormFetcher::IsBlacklisted() const {
  if (client_->GetPasswordFeatureManager()->IsOptedInForAccountStorage() &&
      client_->GetPasswordFeatureManager()->GetDefaultPasswordStore() ==
          PasswordForm::Store::kAccountStore) {
    return is_blacklisted_in_account_store_;
  }
  return is_blacklisted_in_profile_store_;
}

bool MultiStoreFormFetcher::IsMovingBlocked(
    const autofill::GaiaIdHash& destination,
    const base::string16& username) const {
  for (const std::vector<std::unique_ptr<PasswordForm>>* matches_vector :
       {&federated_, &non_federated_}) {
    for (const auto& form : *matches_vector) {
      // Only local entries can be moved to the account store (though
      // account store matches should never have |moving_blocked_for_list|
      // entries anyway).
      if (form->IsUsingAccountStore())
        continue;
      // Ignore PSL matches for blocking moving.
      if (form->is_public_suffix_match)
        continue;
      if (form->username_value != username)
        continue;
      if (base::Contains(form->moving_blocked_for_list, destination))
        return true;
    }
  }
  return false;
}

std::unique_ptr<FormFetcher> MultiStoreFormFetcher::Clone() {
  std::unique_ptr<FormFetcher> fetcher_ptr = FormFetcherImpl::Clone();
  MultiStoreFormFetcher& fetcher =
      *static_cast<MultiStoreFormFetcher*>(fetcher_ptr.get());
  fetcher.is_blacklisted_in_account_store_ = is_blacklisted_in_account_store_;
  fetcher.is_blacklisted_in_profile_store_ = is_blacklisted_in_profile_store_;
  return fetcher_ptr;
}

void MultiStoreFormFetcher::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  // This class overrides OnGetPasswordStoreResultsFrom() (the version of this
  // method that also receives the originating store), so the store-less version
  // never gets called.
  NOTREACHED();
}

void MultiStoreFormFetcher::OnGetPasswordStoreResultsFrom(
    PasswordStore* store,
    std::vector<std::unique_ptr<PasswordForm>> results) {
  DCHECK_EQ(State::WAITING, state_);
  DCHECK_GT(wait_counter_, 0);

  if (should_migrate_http_passwords_ && results.empty() &&
      form_digest_.url.SchemeIs(url::kHttpsScheme)) {
    http_migrators_[store] = std::make_unique<HttpPasswordStoreMigrator>(
        url::Origin::Create(form_digest_.url), store,
        client_->GetNetworkContext(), this);
    // The migrator will call us back at ProcessMigratedForms().
    return;
  }

  AggregatePasswordStoreResults(std::move(results));
}

void MultiStoreFormFetcher::AggregatePasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  // Store the results.
  for (auto& form : results)
    partial_results_.push_back(std::move(form));

  // If we're still awaiting more results, nothing else to do.
  if (--wait_counter_ > 0)
    return;

  if (need_to_refetch_) {
    // The received results are no longer up to date, need to re-request.
    state_ = State::NOT_WAITING;
    partial_results_.clear();
    Fetch();
    need_to_refetch_ = false;
    return;
  }

  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger(client_->GetLogManager())
        .LogNumber(Logger::STRING_ON_GET_STORE_RESULTS_METHOD, results.size());
  }
  ProcessPasswordStoreResults(std::move(partial_results_));
}

void MultiStoreFormFetcher::ProcessMigratedForms(
    std::vector<std::unique_ptr<PasswordForm>> forms) {
  // The migration from HTTP to HTTPS (within the profile store) was finished.
  // Continue processing with the migrated results.
  AggregatePasswordStoreResults(std::move(forms));
}

void MultiStoreFormFetcher::OnGetCompromisedCredentials(
    std::vector<CompromisedCredentials> compromised_credentials) {
  // Both the profile and account store has been queried. Therefore, append the
  // received credentials to the existing ones.
  base::ranges::move(compromised_credentials,
                     std::back_inserter(compromised_credentials_));
}

void MultiStoreFormFetcher::SplitResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  // Compute the |is_blacklisted_in_profile_store_| and
  // |is_blacklisted_in_account_store_| and then delegate the rest to splitting
  // to FormFetcherImpl::SplitResults().
  is_blacklisted_in_profile_store_ = false;
  is_blacklisted_in_account_store_ = false;
  for (auto& result : results) {
    if (!result->blocked_by_user)
      continue;
    // Ignore PSL matches for blacklisted entries.
    if (result->is_public_suffix_match)
      continue;
    if (result->IsUsingAccountStore())
      is_blacklisted_in_account_store_ = true;
    else
      is_blacklisted_in_profile_store_ = true;
  }
  FormFetcherImpl::SplitResults(std::move(results));
}

}  // namespace password_manager
