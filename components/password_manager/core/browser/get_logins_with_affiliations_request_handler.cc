// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/get_logins_with_affiliations_request_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"

#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"

namespace password_manager {

namespace {

// Number of time 'forms_received_' closure should be called before executing.
// Once for perfect matches and once for affiliations.
constexpr int kCallsNumber = 2;

}  // namespace

GetLoginsWithAffiliationsRequestHandler::
    GetLoginsWithAffiliationsRequestHandler(
        const PasswordFormDigest& form,
        base::WeakPtr<PasswordStoreConsumer> consumer,
        PasswordStoreInterface* store)
    : requested_digest_(form), consumer_(std::move(consumer)), store_(store) {
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

base::OnceCallback<
    std::vector<PasswordFormDigest>(const std::vector<std::string>&)>
GetLoginsWithAffiliationsRequestHandler::AffiliationsClosure() {
  return base::BindOnce(
      &GetLoginsWithAffiliationsRequestHandler::HandleAffiliationsReceived,
      this);
}

base::OnceCallback<void(LoginsResult)>
GetLoginsWithAffiliationsRequestHandler::AffiliatedLoginsClosure() {
  return base::BindOnce(
      &GetLoginsWithAffiliationsRequestHandler::HandleAffiliatedLoginsReceived,
      this);
}

void GetLoginsWithAffiliationsRequestHandler::HandleLoginsForFormReceived(
    std::vector<std::unique_ptr<PasswordForm>> logins) {
  for (const auto& form : logins) {
    form->is_public_suffix_match =
        form->signon_realm == requested_digest_.signon_realm
            ? false
            : IsPublicSuffixDomainMatch(form->signon_realm,
                                        requested_digest_.signon_realm);
  }
  results_.insert(results_.end(), std::make_move_iterator(logins.begin()),
                  std::make_move_iterator(logins.end()));
  forms_received_.Run();
}

std::vector<PasswordFormDigest>
GetLoginsWithAffiliationsRequestHandler::HandleAffiliationsReceived(
    const std::vector<std::string>& realms) {
  std::vector<PasswordFormDigest> forms;
  for (const auto& realm : realms) {
    // The PSL forms are requested in the main request.
    if (!IsPublicSuffixDomainMatch(realm, requested_digest_.signon_realm))
      forms.emplace_back(PasswordForm::Scheme::kHtml, realm, GURL(realm));
  }
  affiliations_ = base::flat_set<std::string>(realms.begin(), realms.end());
  return forms;
}

void GetLoginsWithAffiliationsRequestHandler::HandleAffiliatedLoginsReceived(
    std::vector<std::unique_ptr<PasswordForm>> logins) {
  password_manager_util::TrimUsernameOnlyCredentials(&logins);
  // PasswordStore must request only exact matches for the domains filtered in
  // HandleAffiliationsReceived.
  for (const auto& form : logins)
    DCHECK(!form->is_public_suffix_match);
  std::transform(logins.begin(), logins.end(), std::back_inserter(results_),
                 [](auto& form) {
                   return std::move(form);
                 });
  forms_received_.Run();
}

void GetLoginsWithAffiliationsRequestHandler::NotifyConsumer() {
  if (!consumer_)
    return;
  // PSL matches can also be affiliation matches.
  for (const auto& form : results_) {
    if (form->signon_realm != requested_digest_.signon_realm &&
        base::Contains(affiliations_, form->signon_realm)) {
      form->is_affiliation_based_match = true;
    }
  }
  consumer_->OnGetPasswordStoreResultsFrom(store_, std::move(results_));
}

}  // namespace password_manager
