// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_pending_prevent_silent_access_task.h"

#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/password_store_util.h"

namespace password_manager {

CredentialManagerPendingPreventSilentAccessTask::
    CredentialManagerPendingPreventSilentAccessTask(
        CredentialManagerPendingPreventSilentAccessTaskDelegate* delegate)
    : delegate_(delegate), pending_requests_(0) {}

CredentialManagerPendingPreventSilentAccessTask::
    ~CredentialManagerPendingPreventSilentAccessTask() = default;

void CredentialManagerPendingPreventSilentAccessTask::AddOrigin(
    const PasswordFormDigest& form_digest) {
  delegate_->GetProfilePasswordStore()->GetLogins(
      form_digest, weak_ptr_factory_.GetWeakPtr());
  pending_requests_++;
  if (PasswordStoreInterface* account_store =
          delegate_->GetAccountPasswordStore()) {
    account_store->GetLogins(form_digest, weak_ptr_factory_.GetWeakPtr());
    pending_requests_++;
  }
}

void CredentialManagerPendingPreventSilentAccessTask::
    OnGetPasswordStoreResultsOrErrorFrom(PasswordStoreInterface* store,
                                         LoginsResultOrError results_or_error) {
  LoginsResult results =
      GetLoginsOrEmptyListOnFailure(std::move(results_or_error));
  for (auto& form : results) {
    if (form.match_type == PasswordForm::MatchType::kGrouped ||
        form.blocked_by_user) {
      continue;
    }
    if (!form.skip_zero_click) {
      form.skip_zero_click = true;
      store->UpdateLogin(std::move(form));
    }
  }
  pending_requests_--;
  if (!pending_requests_) {
    delegate_->DoneRequiringUserMediation();
  }
}

}  // namespace password_manager
