// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DUPLICATE_PERMISSION_CLEANER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DUPLICATE_PERMISSION_CLEANER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/actor_login/actor_login_permission_service.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "url/origin.h"

namespace actor_login {

class ActorLoginDuplicatePermissionCleaner
    : public password_manager::PasswordStoreConsumer {
 public:
  ActorLoginDuplicatePermissionCleaner(
      const Credential& credential,
      scoped_refptr<password_manager::PasswordStoreInterface> profile_store,
      scoped_refptr<password_manager::PasswordStoreInterface> account_store,
      ActorLoginPermissionService* permission_service);

  ~ActorLoginDuplicatePermissionCleaner() override;

  void Start(bool check_federated_credentials, base::OnceClosure done_callback);

  // password_manager::PasswordStoreConsumer:
  void OnGetPasswordStoreResultsOrErrorFrom(
      password_manager::PasswordStoreInterface* store,
      password_manager::LoginsResultOrError results_or_error) override;

 private:
  void OnFederatedPermissionsListed(
      std::vector<FederatedPermission> permissions);
  void ClearPasswordPermissions();

  // Credential for which the new permission was saved. Its permission
  // should not be removed.
  Credential credential_;

  scoped_refptr<password_manager::PasswordStoreInterface> profile_store_;
  scoped_refptr<password_manager::PasswordStoreInterface> account_store_;
  raw_ptr<ActorLoginPermissionService> permission_service_ = nullptr;

  // Called when all deletions have been scheduled.
  base::OnceClosure done_callback_;

  // Callbacks for when the password and federated fetches are complete.
  base::RepeatingClosure passwords_done_callback_;
  base::RepeatingClosure federated_done_callback_;

  int pending_password_fetches_ = 0;
  std::vector<password_manager::PasswordForm> pending_matches_;

  base::WeakPtrFactory<ActorLoginDuplicatePermissionCleaner> weak_ptr_factory_{
      this};
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DUPLICATE_PERMISSION_CLEANER_H_
