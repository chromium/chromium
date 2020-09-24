// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_pending_prevent_silent_access_task.h"

#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

CredentialManagerPendingPreventSilentAccessTask::
    CredentialManagerPendingPreventSilentAccessTask(
        CredentialManagerPendingPreventSilentAccessTaskDelegate* delegate)
    : delegate_(delegate), pending_requests_(0) {}

CredentialManagerPendingPreventSilentAccessTask::
    ~CredentialManagerPendingPreventSilentAccessTask() = default;

void CredentialManagerPendingPreventSilentAccessTask::AddOrigin(
    const PasswordStore::FormDigest& form_digest) {
  delegate_->GetProfilePasswordStore()->GetLogins(form_digest, this);
  pending_requests_++;
  if (PasswordStore* account_store = delegate_->GetAccountPasswordStore()) {
    account_store->GetLogins(form_digest, this);
    pending_requests_++;
  }
}

void CredentialManagerPendingPreventSilentAccessTask::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  // This class overrides OnGetPasswordStoreResultsFrom() (the version of this
  // method that also receives the originating store), so the store-less version
  // never gets called.
  NOTREACHED();
}

void CredentialManagerPendingPreventSilentAccessTask::
    OnGetPasswordStoreResultsFrom(
        PasswordStore* store,
        std::vector<std::unique_ptr<PasswordForm>> results) {
  for (const auto& form : results) {
    if (!form->skip_zero_click) {
      form->skip_zero_click = true;
      store->UpdateLogin(*form);
    }
  }
  pending_requests_--;
  if (!pending_requests_)
    delegate_->DoneRequiringUserMediation();
}

}  // namespace password_manager
