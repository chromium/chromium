// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_pending_prevent_silent_access_task.h"

#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"

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

void CredentialManagerPendingPreventSilentAccessTask::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  // This class overrides OnGetPasswordStoreResultsFrom() (the version of this
  // method that also receives the originating store), so the store-less version
  // never gets called.
  NOTREACHED_IN_MIGRATION();
}

void CredentialManagerPendingPreventSilentAccessTask::
    OnGetPasswordStoreResultsFrom(
        PasswordStoreInterface* store,
        std::vector<std::unique_ptr<PasswordForm>> results) {
  for (const auto& form : results) {
    if (form->match_type == PasswordForm::MatchType::kGrouped ||
        form->blocked_by_user) {
      continue;
    }
    if (!form->skip_zero_click) {
      form->skip_zero_click = true;
      store->UpdateLogin(*form);
    }
  }
  pending_requests_--;
  if (!pending_requests_) {
    delegate_->DoneRequiringUserMediation();
  }
}

}  // namespace password_manager
