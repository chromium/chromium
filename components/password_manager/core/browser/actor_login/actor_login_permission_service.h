// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_PERMISSION_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_PERMISSION_SERVICE_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"

namespace actor_login {

// Represents a user's federated credential (FedCM, OpenID Connect) permission.
struct FederatedPermission {
  FederatedPermission();
  FederatedPermission(const FederatedPermission&);
  FederatedPermission& operator=(const FederatedPermission&);
  ~FederatedPermission();

  std::string idp_origin;
  std::string rp_embedder_origin;
  std::string rp_requester_origin;
  std::string chosen_account_id;
  std::string chosen_account_email;
  std::vector<std::string> affiliated_requester_origins;
};

using ListPermissionsResult =
    base::OnceCallback<void(std::vector<FederatedPermission>)>;

// Manages actor login permissions.
// Currently this only manages federated permissions but the long-term goal is
// to manage all actor login permissions through this service.
class ActorLoginPermissionService : public KeyedService {
 public:
  ~ActorLoginPermissionService() override = default;

  // Lists actor login permissions for the primary profile.
  virtual void ListAllPermissions(ListPermissionsResult callback) = 0;
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_PERMISSION_SERVICE_H_
