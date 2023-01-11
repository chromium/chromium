// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PENDING_REQUEST_TASK_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PENDING_REQUEST_TASK_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/http_password_store_migrator.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

struct CredentialInfo;
struct PasswordForm;
class PasswordManagerClient;

using SendCredentialCallback =
    base::OnceCallback<void(const CredentialInfo& credential)>;

enum class StoresToQuery { kProfileStore, kProfileAndAccountStores };
// Sends credentials retrieved from the PasswordStoreInterface to
// CredentialManager API clients and retrieves embedder-dependent information.
class CredentialManagerPendingRequestTaskDelegate {
 public:
  // Determines whether zero-click sign-in is allowed.
  virtual bool IsZeroClickAllowed() const = 0;

  // Retrieves the current page origin.
  virtual url::Origin GetOrigin() const = 0;

  // Returns the PasswordManagerClient.
  virtual PasswordManagerClient* client() const = 0;

  // Sends a credential to JavaScript.
  virtual void SendCredential(SendCredentialCallback send_callback,
                              const CredentialInfo& credential) = 0;

  // Updates |skip_zero_click| for |form| in the PasswordStoreInterface if
  // required. Sends a credential to JavaScript.
  virtual void SendPasswordForm(SendCredentialCallback send_callback,
                                CredentialMediationRequirement mediation,
                                const PasswordForm* form) = 0;
};

// Retrieves credentials from the PasswordStoreInterface.
class CredentialManagerPendingRequestTask
    : public PasswordStoreConsumer,
      public HttpPasswordStoreMigrator::Consumer {
 public:
  CredentialManagerPendingRequestTask(
      CredentialManagerPendingRequestTaskDelegate* delegate,
      SendCredentialCallback callback,
      CredentialMediationRequirement mediation,
      bool include_passwords,
      const std::vector<GURL>& request_federations,
      StoresToQuery stores_to_query);
  CredentialManagerPendingRequestTask(
      const CredentialManagerPendingRequestTask&) = delete;
  CredentialManagerPendingRequestTask& operator=(
      const CredentialManagerPendingRequestTask&) = delete;
  ~CredentialManagerPendingRequestTask() override;

  const url::Origin& origin() const { return origin_; }

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;
  void OnGetPasswordStoreResultsFrom(
      PasswordStoreInterface* store,
      std::vector<std::unique_ptr<PasswordForm>> results) override;
  base::WeakPtr<PasswordStoreConsumer> GetWeakPtr();

 private:
  // HttpPasswordStoreMigrator::Consumer:
  void ProcessMigratedForms(
      std::vector<std::unique_ptr<PasswordForm>> forms) override;

  void AggregatePasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results);

  void ProcessForms(std::vector<std::unique_ptr<PasswordForm>> results);

  raw_ptr<CredentialManagerPendingRequestTaskDelegate> delegate_;  // Weak;
  SendCredentialCallback send_callback_;
  const CredentialMediationRequirement mediation_;
  const url::Origin origin_;
  const bool include_passwords_;
  std::set<std::string> federations_;
  int expected_stores_to_respond_;
  // In case of querying both the profile and account stores, it contains the
  // partial results received from one store until the second store responds and
  // then all results are processed.
  std::vector<std::unique_ptr<PasswordForm>> partial_results_;

  base::flat_map<PasswordStoreInterface*,
                 std::unique_ptr<HttpPasswordStoreMigrator>>
      http_migrators_;

  base::WeakPtrFactory<CredentialManagerPendingRequestTask> weak_ptr_factory_{
      this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PENDING_REQUEST_TASK_H_
