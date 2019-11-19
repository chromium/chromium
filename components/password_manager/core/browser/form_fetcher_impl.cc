// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_fetcher_impl.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "build/build_config.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/multi_store_form_fetcher.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/common/password_manager_features.h"

using autofill::PasswordForm;

using Logger = autofill::SavePasswordProgressLogger;

namespace password_manager {

namespace {

// Create a vector of const PasswordForm from a vector of
// unique_ptr<PasswordForm> by applying get() item-wise.
std::vector<const PasswordForm*> MakeWeakCopies(
    const std::vector<std::unique_ptr<PasswordForm>>& owning) {
  std::vector<const PasswordForm*> result(owning.size());
  std::transform(
      owning.begin(), owning.end(), result.begin(),
      [](const std::unique_ptr<PasswordForm>& ptr) { return ptr.get(); });
  return result;
}

// Create a vector of unique_ptr<PasswordForm> from another such vector by
// copying the pointed-to forms.
std::vector<std::unique_ptr<PasswordForm>> MakeCopies(
    const std::vector<std::unique_ptr<PasswordForm>>& source) {
  std::vector<std::unique_ptr<PasswordForm>> result(source.size());
  std::transform(source.begin(), source.end(), result.begin(),
                 [](const std::unique_ptr<PasswordForm>& ptr) {
                   return std::make_unique<PasswordForm>(*ptr);
                 });
  return result;
}

}  // namespace

// static
std::unique_ptr<FormFetcherImpl> FormFetcherImpl::CreateFormFetcherImpl(
    PasswordStore::FormDigest form_digest,
    const PasswordManagerClient* client,
    bool should_migrate_http_passwords) {
  return base::FeatureList::IsEnabled(
             password_manager::features::kEnablePasswordsAccountStorage)
             ? std::make_unique<MultiStoreFormFetcher>(
                   std::move(form_digest), client,
                   should_migrate_http_passwords)
             : std::make_unique<FormFetcherImpl>(std::move(form_digest), client,
                                                 should_migrate_http_passwords);
}

FormFetcherImpl::FormFetcherImpl(PasswordStore::FormDigest form_digest,
                                 const PasswordManagerClient* client,
                                 bool should_migrate_http_passwords)
    : form_digest_(std::move(form_digest)),
      client_(client),
      should_migrate_http_passwords_(should_migrate_http_passwords) {}

FormFetcherImpl::~FormFetcherImpl() = default;

void FormFetcherImpl::AddConsumer(FormFetcher::Consumer* consumer) {
  DCHECK(consumer);
  consumers_.AddObserver(consumer);
  if (state_ == State::NOT_WAITING)
    consumer->OnFetchCompleted();
}

void FormFetcherImpl::RemoveConsumer(FormFetcher::Consumer* consumer) {
  DCHECK(consumers_.HasObserver(consumer));
  consumers_.RemoveObserver(consumer);
}

FormFetcherImpl::State FormFetcherImpl::GetState() const {
  return state_;
}

const std::vector<InteractionsStats>& FormFetcherImpl::GetInteractionsStats()
    const {
  return interactions_stats_;
}

std::vector<const PasswordForm*> FormFetcherImpl::GetNonFederatedMatches()
    const {
  return MakeWeakCopies(non_federated_);
}

std::vector<const PasswordForm*> FormFetcherImpl::GetFederatedMatches() const {
  return MakeWeakCopies(federated_);
}

bool FormFetcherImpl::IsBlacklisted() const {
  return is_blacklisted_;
}

const std::vector<const PasswordForm*>& FormFetcherImpl::GetAllRelevantMatches()
    const {
  return non_federated_same_scheme_;
}

const std::vector<const PasswordForm*>& FormFetcherImpl::GetBestMatches()
    const {
  return best_matches_;
}

const PasswordForm* FormFetcherImpl::GetPreferredMatch() const {
  return preferred_match_;
}

void FormFetcherImpl::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  DCHECK_EQ(State::WAITING, state_);

  if (need_to_refetch_) {
    // The received results are no longer up to date, need to re-request.
    state_ = State::NOT_WAITING;
    Fetch();
    need_to_refetch_ = false;
    return;
  }

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_ON_GET_STORE_RESULTS_METHOD);
    logger->LogNumber(Logger::STRING_NUMBER_RESULTS, results.size());
  }

  if (should_migrate_http_passwords_ && results.empty() &&
      form_digest_.origin.SchemeIs(url::kHttpsScheme)) {
    http_migrator_ = std::make_unique<HttpPasswordStoreMigrator>(
        form_digest_.origin, client_, this);
    return;
  }

  ProcessPasswordStoreResults(std::move(results));
}

void FormFetcherImpl::OnGetSiteStatistics(
    std::vector<InteractionsStats> stats) {
  // On Windows the password request may be resolved after the statistics due to
  // importing from IE.
  interactions_stats_ = std::move(stats);
}

void FormFetcherImpl::ProcessMigratedForms(
    std::vector<std::unique_ptr<PasswordForm>> forms) {
  ProcessPasswordStoreResults(std::move(forms));
}

void FormFetcherImpl::Fetch() {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_FETCH_METHOD);
    logger->LogNumber(Logger::STRING_FORM_FETCHER_STATE,
                      static_cast<int>(state_));
  }

  if (state_ == State::WAITING) {
    // There is currently a password store query in progress, need to re-fetch
    // store results later.
    need_to_refetch_ = true;
    return;
  }

  PasswordStore* password_store = client_->GetProfilePasswordStore();
  if (!password_store) {
    if (logger)
      logger->LogMessage(Logger::STRING_NO_STORE);
    NOTREACHED();
    return;
  }
  state_ = State::WAITING;
  password_store->GetLogins(form_digest_, this);

// The statistics isn't needed on mobile, only on desktop. Let's save some
// processor cycles.
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  // The statistics is needed for the "Save password?" bubble.
  password_store->GetSiteStats(form_digest_.origin.GetOrigin(), this);
#endif
}

std::unique_ptr<FormFetcher> FormFetcherImpl::Clone() {
  // Create the copy without the "HTTPS migration" activated. If it was needed,
  // then it was done by |this| already.
  auto result = std::make_unique<FormFetcherImpl>(form_digest_, client_, false);

  if (state_ != State::NOT_WAITING) {
    // There are no store results to copy, trigger a Fetch on the clone instead.
    result->Fetch();
    return std::move(result);
  }

  result->non_federated_ = MakeCopies(non_federated_);
  result->federated_ = MakeCopies(federated_);
  result->is_blacklisted_ = is_blacklisted_;
  password_manager_util::FindBestMatches(
      MakeWeakCopies(result->non_federated_), form_digest_.scheme,
      sort_matches_by_date_last_used_, &result->non_federated_same_scheme_,
      &result->best_matches_, &result->preferred_match_);

  result->interactions_stats_ = interactions_stats_;
  result->state_ = state_;
  result->need_to_refetch_ = need_to_refetch_;

  return std::move(result);
}

void FormFetcherImpl::ProcessPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  DCHECK_EQ(State::WAITING, state_);
  state_ = State::NOT_WAITING;
  SplitResults(std::move(results));

  password_manager_util::FindBestMatches(
      MakeWeakCopies(non_federated_), form_digest_.scheme,
      sort_matches_by_date_last_used_, &non_federated_same_scheme_,
      &best_matches_, &preferred_match_);

  for (auto& consumer : consumers_)
    consumer.OnFetchCompleted();
}

void FormFetcherImpl::SplitResults(
    std::vector<std::unique_ptr<PasswordForm>> forms) {
  is_blacklisted_ = false;
  non_federated_.clear();
  federated_.clear();
  for (auto& form : forms) {
    if (form->blacklisted_by_user) {
      // Ignore PSL matches for blacklisted entries.
      if (!form->is_public_suffix_match) {
        is_blacklisted_ = true;
      }
    } else if (form->IsFederatedCredential()) {
      federated_.push_back(std::move(form));
    } else {
      non_federated_.push_back(std::move(form));
    }
  }
}

}  // namespace password_manager
