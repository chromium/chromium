// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_PERMISSIONS_MANAGER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_PERMISSIONS_MANAGER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/password_manager/core/browser/actor_login/actor_login_permissions_manager.h"
#include "components/password_manager/core/browser/actor_login/federated_permission.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

namespace affiliations {
class AffiliationService;
}  // namespace affiliations

namespace syncer {
class SyncService;
}  // namespace syncer

namespace password_manager {
class PasswordStoreInterface;
}  // namespace password_manager

namespace actor_login {
class ActorLoginPermissionService;

class ActorLoginPermissionsManagerImpl
    : public ActorLoginPermissionsManager,
      public password_manager::SavedPasswordsPresenter::Observer {
 public:
  ActorLoginPermissionsManagerImpl(
      affiliations::AffiliationService* affiliation_service,
      ActorLoginPermissionService* actor_login_permission_service,
      scoped_refptr<password_manager::PasswordStoreInterface> profile_store,
      scoped_refptr<password_manager::PasswordStoreInterface> account_store);
  ~ActorLoginPermissionsManagerImpl() override;

  // ActorLoginPermissionsManager:
  void AddObserver(ActorLoginPermissionsManager::Observer* observer) override;
  void RemoveObserver(
      ActorLoginPermissionsManager::Observer* observer) override;
  void RevokePermission(const std::string& signon_realm,
                        const std::string& username,
                        base::OnceCallback<void(bool)> callback) override;
  void GetAllPermissions(const syncer::SyncService* sync_service,
                         GetAllPermissionsResult callback) override;

#if defined(UNIT_TEST)
  bool IsWaitingForPasswordStore() const {
    return presenter_.IsWaitingForPasswordStore();
  }
#endif

 private:
  // SavedPasswordsPresenter::Observer:
  void OnSavedPasswordsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  void NotifyObservers();
  void OnPermissionDeleted(base::OnceCallback<void(bool)> callback,
                           bool success);
  void OnSavedPasswordsPresenterInitialized();
  void OnFederatedPermissionsReceived(
      const syncer::SyncService* sync_service,
      GetAllPermissionsResult callback,
      std::vector<FederatedPermission> federated_permissions);

  password_manager::SavedPasswordsPresenter presenter_;
  base::ObserverList<ActorLoginPermissionsManager::Observer> observers_;
  // Any calls to `GetAllPermissions()` made before password store
  // initialization is complete are preserved here to be executed afterwards by
  // chaining in FIFO order. Callback is invoked inside
  // `OnSavedPasswordsPresenterInitialized()`.
  base::OnceClosure pending_get_permissions_callback_ = base::DoNothing();
  raw_ptr<ActorLoginPermissionService> actor_login_permission_service_ =
      nullptr;
  // The last list of federated permissions received from the federated
  // permissions service. Used to check if we need to revoke federated
  // permission in `RevokePermission`.
  // Since the manager is used in the UI, it's impossible to trigger revoke
  // request before `GetAllPermissions` (because the delete button is attached
  // to the permission list entry).
  std::vector<FederatedPermission> last_federated_permissions_;

  base::WeakPtrFactory<ActorLoginPermissionsManagerImpl> weak_ptr_factory_{
      this};
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_PERMISSIONS_MANAGER_IMPL_H_
