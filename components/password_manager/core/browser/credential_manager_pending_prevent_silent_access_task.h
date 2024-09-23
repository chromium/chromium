// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PENDING_PREVENT_SILENT_ACCESS_TASK_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PENDING_PREVENT_SILENT_ACCESS_TASK_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"

namespace password_manager {

// Handles mediation completion and retrieves embedder-dependent services.
class CredentialManagerPendingPreventSilentAccessTaskDelegate {
 public:
  virtual ~CredentialManagerPendingPreventSilentAccessTaskDelegate() = default;

  // Retrieves the profile PasswordStoreInterface.
  virtual PasswordStoreInterface* GetProfilePasswordStore() = 0;
  // Retrieves the account PasswordStoreInterface.
  virtual PasswordStoreInterface* GetAccountPasswordStore() = 0;

  // Finishes mediation tasks.
  virtual void DoneRequiringUserMediation() = 0;
};

// Notifies the password store that a list of origins require user mediation.
class CredentialManagerPendingPreventSilentAccessTask
    : public PasswordStoreConsumer {
 public:
  explicit CredentialManagerPendingPreventSilentAccessTask(
      CredentialManagerPendingPreventSilentAccessTaskDelegate* delegate);
  CredentialManagerPendingPreventSilentAccessTask(
      const CredentialManagerPendingPreventSilentAccessTask&) = delete;
  CredentialManagerPendingPreventSilentAccessTask& operator=(
      const CredentialManagerPendingPreventSilentAccessTask&) = delete;
  ~CredentialManagerPendingPreventSilentAccessTask() override;

  // Adds an origin to require user mediation.
  void AddOrigin(const PasswordFormDigest& form_digest);

 private:
  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;
  void OnGetPasswordStoreResultsFrom(
      PasswordStoreInterface* store,
      std::vector<std::unique_ptr<PasswordForm>> results) override;

  const raw_ptr<CredentialManagerPendingPreventSilentAccessTaskDelegate>
      delegate_;  // Weak.

  // Number of password store requests to be resolved.
  int pending_requests_;

  base::WeakPtrFactory<CredentialManagerPendingPreventSilentAccessTask>
      weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PENDING_PREVENT_SILENT_ACCESS_TASK_H_
