// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_PERMISSION_SERVICE_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_PERMISSION_SERVICE_IMPL_H_

#include "components/password_manager/core/browser/actor_login/actor_login_permission_service.h"

namespace actor_login {

class ActorLoginPermissionServiceImpl : public ActorLoginPermissionService {
 public:
  ActorLoginPermissionServiceImpl();
  ActorLoginPermissionServiceImpl(const ActorLoginPermissionServiceImpl&) =
      delete;
  ActorLoginPermissionServiceImpl& operator=(
      const ActorLoginPermissionServiceImpl&) = delete;
  ~ActorLoginPermissionServiceImpl() override;

  // ActorLoginPermissionService:
  void ListAllPermissions(ListPermissionsResult callback) override;
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_PERMISSION_SERVICE_IMPL_H_
