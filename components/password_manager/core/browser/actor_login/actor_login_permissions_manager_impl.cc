// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/actor_login_permissions_manager_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/password_manager/core/browser/actor_login/actor_login_permission_service.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/sync/service/sync_service.h"

namespace actor_login {

namespace {
void MergePermissions(
    const base::flat_set<password_manager::ActorLoginPermission>&
        password_permissions,
    ActorLoginPermissionsManager::GetAllPermissionsResult callback,
    std::vector<FederatedPermission> federated_permissions) {
  base::flat_set<password_manager::ActorLoginPermission> merged_permissions =
      password_permissions;
  // TODO(crbug.com/481214101): Merge permissions.
  std::move(callback).Run(std::move(merged_permissions));
}
}  // namespace

ActorLoginPermissionsManagerImpl::ActorLoginPermissionsManagerImpl(
    affiliations::AffiliationService* affiliation_service,
    ActorLoginPermissionService* actor_login_permission_service,
    scoped_refptr<password_manager::PasswordStoreInterface> profile_store,
    scoped_refptr<password_manager::PasswordStoreInterface> account_store)
    : presenter_(affiliation_service,
                 std::move(profile_store),
                 std::move(account_store)),
      actor_login_permission_service_(actor_login_permission_service) {
  presenter_.Init();
  presenter_.AddObserver(this);
}

ActorLoginPermissionsManagerImpl::~ActorLoginPermissionsManagerImpl() {
  presenter_.RemoveObserver(this);
}

void ActorLoginPermissionsManagerImpl::AddObserver(
    ActorLoginPermissionsManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void ActorLoginPermissionsManagerImpl::RemoveObserver(
    ActorLoginPermissionsManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ActorLoginPermissionsManagerImpl::RevokePermission(
    const std::string& signon_realm) {
  presenter_.RevokeActorLoginPermission(signon_realm);
}

void ActorLoginPermissionsManagerImpl::GetAllPermissions(
    const syncer::SyncService* sync_service,
    GetAllPermissionsResult callback) {
  base::flat_set<password_manager::ActorLoginPermission>
      saved_passwords_permissions =
          presenter_.GetActorLoginPermissions(sync_service);
  actor_login_permission_service_->ListAllPermissions(
      base::BindOnce(&MergePermissions, std::move(saved_passwords_permissions),
                     std::move(callback)));
}

void ActorLoginPermissionsManagerImpl::OnSavedPasswordsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  for (ActorLoginPermissionsManager::Observer& observer : observers_) {
    observer.OnPermissionsChanged();
  }
}

}  // namespace actor_login
