// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_PERMISSION_SERVICE_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_PERMISSION_SERVICE_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "components/password_manager/core/browser/actor_login/actor_login_permission_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "url/origin.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace actor_login {

class ActorLoginPermissionServiceImpl : public ActorLoginPermissionService {
 public:
  ActorLoginPermissionServiceImpl(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ActorLoginPermissionServiceImpl(const ActorLoginPermissionServiceImpl&) =
      delete;
  ActorLoginPermissionServiceImpl& operator=(
      const ActorLoginPermissionServiceImpl&) = delete;
  ~ActorLoginPermissionServiceImpl() override;

  // ActorLoginPermissionService:
  void ListPermissions(const std::vector<FederatedOrigins>& origins,
                       ListPermissionsResult callback) override;
  void ListAllPermissions(ListPermissionsResult callback) override;
  void DeletePermission(const url::Origin& embedder_origin,
                        DeletePermissionResult callback) override;
  void GrantPermission(const FederatedPermission& permission,
                       GrantPermissionResult callback) override;

 private:
  class Request;

  void StartRequest(std::unique_ptr<Request> request);

  std::vector<FederatedPermission> OnListRequestCompleted(
      Request* request,
      std::optional<std::string> response_body);
  bool OnGenericRequestCompleted(Request* request,
                                 std::optional<std::string> response_body);

  // `KeyedService` that this service depends on.
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  std::vector<std::unique_ptr<Request>> pending_requests_;
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_PERMISSION_SERVICE_IMPL_H_
