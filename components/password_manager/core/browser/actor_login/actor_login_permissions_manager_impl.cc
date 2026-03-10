// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/actor_login_permissions_manager_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/sync/service/sync_service.h"

namespace actor_login {

ActorLoginPermissionsManagerImpl::ActorLoginPermissionsManagerImpl(
    affiliations::AffiliationService* affiliation_service,
    scoped_refptr<password_manager::PasswordStoreInterface> profile_store,
    scoped_refptr<password_manager::PasswordStoreInterface> account_store)
    : presenter_(affiliation_service,
                 std::move(profile_store),
                 std::move(account_store)) {
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

base::flat_set<password_manager::ActorLoginPermission>
ActorLoginPermissionsManagerImpl::GetAllPermissions(
    const syncer::SyncService* sync_service) {
  return presenter_.GetActorLoginPermissions(sync_service);
}

void ActorLoginPermissionsManagerImpl::OnSavedPasswordsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  for (ActorLoginPermissionsManager::Observer& observer : observers_) {
    observer.OnPermissionsChanged();
  }
}

}  // namespace actor_login
