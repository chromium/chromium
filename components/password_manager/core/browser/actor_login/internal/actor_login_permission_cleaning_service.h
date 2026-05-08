// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_PERMISSION_CLEANING_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_PERMISSION_CLEANING_SERVICE_H_

#include "base/functional/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"

namespace actor_login {

// Service responsible for clearing permissions. It creates and manages
// one-off `ActorLoginDuplicatePermissionCleaner` instances for each request.
class ActorLoginPermissionCleaningService : public KeyedService {
 public:
  ActorLoginPermissionCleaningService() = default;

  ActorLoginPermissionCleaningService(
      const ActorLoginPermissionCleaningService&) = delete;
  ActorLoginPermissionCleaningService& operator=(
      const ActorLoginPermissionCleaningService&) = delete;

  ~ActorLoginPermissionCleaningService() override = default;

  // Starts the asynchronous process of fetching and clearing duplicate
  // permissions.
  // `credential` is used to identify which permission to skip, so as to not
  // remove the newly granted permission. If `check_federated_credentials` is
  // true, delete conflicting federated permissions as well.
  virtual void ClearConflictingPermissions(const Credential& credential,
                                           bool check_federated_credentials,
                                           base::OnceClosure done_callback) = 0;
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_PERMISSION_CLEANING_SERVICE_H_
