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
#include "base/observer_list.h"
#include "components/password_manager/core/browser/actor_login/actor_login_permissions_manager.h"
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

class ActorLoginPermissionsManagerImpl
    : public ActorLoginPermissionsManager,
      public password_manager::SavedPasswordsPresenter::Observer {
 public:
  ActorLoginPermissionsManagerImpl(
      affiliations::AffiliationService* affiliation_service,
      scoped_refptr<password_manager::PasswordStoreInterface> profile_store,
      scoped_refptr<password_manager::PasswordStoreInterface> account_store);
  ~ActorLoginPermissionsManagerImpl() override;

  // ActorLoginPermissionsManager:
  void AddObserver(ActorLoginPermissionsManager::Observer* observer) override;
  void RemoveObserver(
      ActorLoginPermissionsManager::Observer* observer) override;
  void RevokePermission(const std::string& signon_realm) override;
  base::flat_set<password_manager::ActorLoginPermission> GetAllPermissions(
      const syncer::SyncService* sync_service) override;

 private:
  // SavedPasswordsPresenter::Observer:
  void OnSavedPasswordsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  password_manager::SavedPasswordsPresenter presenter_;
  base::ObserverList<ActorLoginPermissionsManager::Observer> observers_;
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_PERMISSIONS_MANAGER_IMPL_H_
