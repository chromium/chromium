// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_PERMISSIONS_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_PERMISSIONS_MANAGER_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list_types.h"
#include "components/password_manager/core/browser/ui/actor_login_permission.h"

namespace syncer {
class SyncService;
}  // namespace syncer

namespace actor_login {

// UI manager for actor login permissions.
//
// This class provides a unified view of permissions for different credential
// types (e.g., passwords, federated credentials).
class ActorLoginPermissionsManager {
 public:
  using GetAllPermissionsResult = base::OnceCallback<void(
      base::flat_set<password_manager::ActorLoginPermission>)>;

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPermissionsChanged() {}
  };

  virtual ~ActorLoginPermissionsManager() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Revokes the actor login permission for the specified `signon_realm` and
  // `username` for all credential types.
  virtual void RevokePermission(const std::string& signon_realm,
                                const std::string& username,
                                base::OnceCallback<void(bool)> callback) = 0;

  // Fetches all actor login permissions and returns them via the callback.
  virtual void GetAllPermissions(const syncer::SyncService* sync_service,
                                 GetAllPermissionsResult callback) = 0;
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_PERMISSIONS_MANAGER_H_
