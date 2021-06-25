// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/get_logins_with_affiliations_request_handler.h"

#include "base/bind.h"
#include "base/callback.h"

#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace password_manager {

namespace {

// Number of time 'forms_received_' closure should be called before executing.
// Once for perfect matches and once for affiliations.
constexpr int kCallsNumber = 2;

}  // namespace

GetLoginsWithAffiliationsRequestHandler::
    GetLoginsWithAffiliationsRequestHandler(
        base::WeakPtr<PasswordStoreConsumer> consumer,
        PasswordStoreInterface* store)
    : consumer_(std::move(consumer)), store_(store) {
  forms_received_ = base::BarrierClosure(
      kCallsNumber,
      base::BindOnce(&GetLoginsWithAffiliationsRequestHandler::NotifyConsumer,
                     base::Unretained(this)));
}

GetLoginsWithAffiliationsRequestHandler::
    ~GetLoginsWithAffiliationsRequestHandler() = default;

base::OnceCallback<void(std::vector<std::unique_ptr<PasswordForm>>)>
GetLoginsWithAffiliationsRequestHandler::LoginsForFormClosure() {
  return base::BindOnce(
      &GetLoginsWithAffiliationsRequestHandler::HandleLoginsForFormReceived,
      this);
}

base::OnceCallback<void(std::vector<std::unique_ptr<PasswordForm>>)>
GetLoginsWithAffiliationsRequestHandler::AffiliatedLoginsClosure() {
  return base::BindOnce(
      &GetLoginsWithAffiliationsRequestHandler::HandleAffiliatedLoginsReceived,
      this);
}

void GetLoginsWithAffiliationsRequestHandler::HandleLoginsForFormReceived(
    std::vector<std::unique_ptr<PasswordForm>> logins) {
  results_.insert(results_.end(), std::make_move_iterator(logins.begin()),
                  std::make_move_iterator(logins.end()));
  forms_received_.Run();
}

void GetLoginsWithAffiliationsRequestHandler::HandleAffiliatedLoginsReceived(
    std::vector<std::unique_ptr<PasswordForm>> logins) {
  password_manager_util::TrimUsernameOnlyCredentials(&logins);
  std::transform(logins.begin(), logins.end(), std::back_inserter(results_),
                 [](auto& form) {
                   form->is_affiliation_based_match = true;
                   return std::move(form);
                 });
  forms_received_.Run();
}

void GetLoginsWithAffiliationsRequestHandler::NotifyConsumer() {
  if (!consumer_)
    return;
  consumer_->OnGetPasswordStoreResultsFrom(store_, std::move(results_));
}

}  // namespace password_manager
