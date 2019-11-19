// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/multi_store_form_fetcher.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/statistics_table.h"

using autofill::PasswordForm;

using Logger = autofill::SavePasswordProgressLogger;

namespace password_manager {

MultiStoreFormFetcher::MultiStoreFormFetcher(
    PasswordStore::FormDigest form_digest,
    const PasswordManagerClient* client,
    bool should_migrate_http_passwords)
    : FormFetcherImpl(form_digest, client, should_migrate_http_passwords) {
  sort_matches_by_date_last_used_ = true;
}

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
  // Issue a fetch from the profile password store using the base class
  // FormFetcherImpl.
  FormFetcherImpl::Fetch();
  if (state_ == State::WAITING) {
    // Fetching from the profile password store is in progress.
    wait_counter_++;
  }

  // Issue a fetch from the account password store if available.
  PasswordStore* account_password_store = client_->GetAccountPasswordStore();
  if (account_password_store) {
    account_password_store->GetLogins(form_digest_, this);
    state_ = State::WAITING;
    wait_counter_++;
  }
}

void MultiStoreFormFetcher::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  DCHECK_EQ(State::WAITING, state_);
  DCHECK_GT(wait_counter_, 0);

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

  // TODO(crbug.com/1002000): implement password store migration.

  ProcessPasswordStoreResults(std::move(partial_results_));
}

}  // namespace password_manager
