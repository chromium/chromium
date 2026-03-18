// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_PERMISSION_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_PERMISSION_SERVICE_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/origin.h"

namespace actor_login {

// Convenience struct for holding a pair of origins used for a FedCM login
// request.
struct FederatedOrigins {
  url::Origin embedder_origin;
  url::Origin requester_origin;
};

// Represents a user's federated credential (FedCM, OpenID Connect) permission.
struct FederatedPermission {
  FederatedPermission();
  FederatedPermission(const FederatedPermission&);
  FederatedPermission& operator=(const FederatedPermission&);
  ~FederatedPermission();

  // IdP origin that identifies the identity provider. For example,
  // "https://accounts.google.com".
  url::Origin idp_origin;
  // Origin of the main frame of the website where actor login was initiated.
  url::Origin rp_embedder_origin;
  // Origin of the iframe that initiated the FedCM flow.
  url::Origin rp_requester_origin;
  // Account ID of the account used for actor login.
  std::string chosen_account_id;
  // Email of the account used for actor login.
  std::string chosen_account_email;
  // Output only. Lists origins that are affiliated with the requester origin.
  std::vector<std::string> affiliated_requester_origins;
};

using ListPermissionsResult =
    base::OnceCallback<void(std::vector<FederatedPermission>)>;

using DeletePermissionResult = base::OnceCallback<void(bool)>;

using GrantPermissionResult = base::OnceCallback<void(bool)>;

// Manages actor login permissions.
// Currently this only manages federated permissions but the long-term goal is
// to manage all actor login permissions through this service.
class ActorLoginPermissionService : public KeyedService {
 public:
  ~ActorLoginPermissionService() override = default;

  // Lists actor login permissions for the primary profile by applying the given
  // origins as filters. Opaque origins are ignored, meaning setting all
  // origins to opaque is equivalent to calling `ListAllPermissions`.
  virtual void ListPermissions(const std::vector<FederatedOrigins>& origins,
                               ListPermissionsResult callback) = 0;

  // Lists all actor login permissions for the primary profile.
  virtual void ListAllPermissions(ListPermissionsResult callback) = 0;

  // Deletes permission for the given embedder origin. If the origin is opaque,
  // the callback will be called with false.
  virtual void DeletePermission(const url::Origin& embedder_origin,
                                DeletePermissionResult callback) = 0;

  // Deletes permission for the given embedder origin and display name. If the
  // origin is opaque, the callback will be called with false.
  // `display_name` is a human-readable name of a federated account, e.g.
  // "example@gmail.com".
  virtual void DeletePermission(const url::Origin& embedder_origin,
                                const std::string& display_name,
                                DeletePermissionResult callback) = 0;

  // Stores `permission` in the permission database for the primary profile.
  // All fields in `permission` are required to be meaningful except for
  // `affiliated_requester_origins`.
  virtual void GrantPermission(const FederatedPermission& permission,
                               GrantPermissionResult callback) = 0;
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_PERMISSION_SERVICE_H_
