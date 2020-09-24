// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PENDING_PREVENT_SILENT_ACCESS_TASK_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PENDING_PREVENT_SILENT_ACCESS_TASK_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace password_manager {

// Handles mediation completion and retrieves embedder-dependent services.
class CredentialManagerPendingPreventSilentAccessTaskDelegate {
 public:
  virtual ~CredentialManagerPendingPreventSilentAccessTaskDelegate() {}

  // Retrieves the profile PasswordStore.
  virtual PasswordStore* GetProfilePasswordStore() = 0;
  // Retrieves the account PasswordStore.
  virtual PasswordStore* GetAccountPasswordStore() = 0;

  // Finishes mediation tasks.
  virtual void DoneRequiringUserMediation() = 0;
};

// Notifies the password store that a list of origins require user mediation.
class CredentialManagerPendingPreventSilentAccessTask
    : public PasswordStoreConsumer {
 public:
  explicit CredentialManagerPendingPreventSilentAccessTask(
      CredentialManagerPendingPreventSilentAccessTaskDelegate* delegate);
  ~CredentialManagerPendingPreventSilentAccessTask() override;

  // Adds an origin to require user mediation.
  void AddOrigin(const PasswordStore::FormDigest& form_digest);

  // PasswordStoreConsumer implementation.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;
  void OnGetPasswordStoreResultsFrom(
      PasswordStore* store,
      std::vector<std::unique_ptr<PasswordForm>> results) override;

 private:
  CredentialManagerPendingPreventSilentAccessTaskDelegate* const
      delegate_;  // Weak.

  // Number of password store requests to be resolved.
  int pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManagerPendingPreventSilentAccessTask);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PENDING_PREVENT_SILENT_ACCESS_TASK_H_
