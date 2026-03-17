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
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/sync/service/sync_service.h"

namespace actor_login {

namespace {
base::flat_set<password_manager::ActorLoginPermission> MergePermissions(
    const base::flat_set<password_manager::ActorLoginPermission>&
        password_permissions,
    std::vector<FederatedPermission> federated_permissions) {
  std::vector<password_manager::ActorLoginPermission> merged_permissions(
      password_permissions.begin(), password_permissions.end());

  std::vector<std::pair<std::u16string, std::string>> password_permissions_keys;
  password_permissions_keys.reserve(password_permissions.size());
  for (const password_manager::ActorLoginPermission& password_permission :
       password_permissions) {
    password_permissions_keys.emplace_back(
        password_permission.username,
        password_permission.domain_info.signon_realm);
  }
  base::flat_set<std::pair<std::u16string, std::string>>
      password_permissions_hash_set(std::move(password_permissions_keys));

  for (const FederatedPermission& federated_permission :
       federated_permissions) {
    std::u16string username =
        base::UTF8ToUTF16(federated_permission.chosen_account_email);
    if (!federated_permission.rp_embedder_origin.opaque() &&
        !password_permissions_hash_set.contains(std::make_pair(
            username,
            federated_permission.rp_embedder_origin.GetURL().spec()))) {
      password_manager::ActorLoginPermission permission;
      permission.username = username;
      permission.domain_info.signon_realm =
          federated_permission.rp_embedder_origin.GetURL().spec();
      permission.domain_info.name = password_manager::GetShownOrigin(
          federated_permission.rp_embedder_origin);
      permission.domain_info.url =
          federated_permission.rp_embedder_origin.GetURL();
      // Favicon is not populated.
      merged_permissions.push_back(std::move(permission));
    }
  }

  // TODO(crbug.com/486089293): Clean permission username conflicts.
  return base::flat_set<password_manager::ActorLoginPermission>(
      std::move(merged_permissions));
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
  // The service is constructed via a factory that returns a nullptr for
  // incognito and guest profiles. The settings page is not accessible for those
  // profiles, so we can assert that the service is valid.
  CHECK(actor_login_permission_service_);
  actor_login_permission_service_->ListAllPermissions(
      base::BindOnce(&MergePermissions, std::move(saved_passwords_permissions))
          .Then(std::move(callback)));
}

void ActorLoginPermissionsManagerImpl::OnSavedPasswordsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  for (ActorLoginPermissionsManager::Observer& observer : observers_) {
    observer.OnPermissionsChanged();
  }
}

}  // namespace actor_login
