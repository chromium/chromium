// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_permission_cleaning_service_impl.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "components/password_manager/core/browser/actor_login/actor_login_permission_service.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_duplicate_permission_cleaner.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"

namespace actor_login {

ActorLoginPermissionCleaningServiceImpl::
    ActorLoginPermissionCleaningServiceImpl(
        ActorLoginPermissionService* permission_service,
        scoped_refptr<password_manager::PasswordStoreInterface> profile_store,
        scoped_refptr<password_manager::PasswordStoreInterface> account_store)
    : permission_service_(permission_service),
      profile_store_(std::move(profile_store)),
      account_store_(std::move(account_store)) {
  CHECK(permission_service_);
}

ActorLoginPermissionCleaningServiceImpl::
    ~ActorLoginPermissionCleaningServiceImpl() = default;

void ActorLoginPermissionCleaningServiceImpl::Shutdown() {
  // Clear any active cleaners immediately (implicitly cancelling ongoing jobs).
  active_cleaners_.clear();
}

void ActorLoginPermissionCleaningServiceImpl::ClearConflictingPermissions(
    const Credential& credential,
    bool check_federated_credentials,
    base::OnceClosure done_callback) {
  auto cleaner = std::make_unique<ActorLoginDuplicatePermissionCleaner>(
      credential, profile_store_, account_store_, permission_service_);

  ActorLoginDuplicatePermissionCleaner* raw_cleaner = cleaner.get();

  auto removal_callback =
      base::BindOnce(&ActorLoginPermissionCleaningServiceImpl::OnCleanerDone,
                     base::Unretained(this), raw_cleaner);

  active_cleaners_.insert(std::move(cleaner));

  raw_cleaner->Start(
      check_federated_credentials,
      std::move(done_callback).Then(std::move(removal_callback)));
}

void ActorLoginPermissionCleaningServiceImpl::OnCleanerDone(
    ActorLoginDuplicatePermissionCleaner* cleaner) {
  auto it = active_cleaners_.find(cleaner);
  if (it != active_cleaners_.end()) {
    active_cleaners_.erase(it);
  }
}

}  // namespace actor_login
