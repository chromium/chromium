// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_permission_service_impl.h"

#include <utility>

#include "base/functional/callback.h"

namespace actor_login {

ActorLoginPermissionServiceImpl::ActorLoginPermissionServiceImpl() = default;

ActorLoginPermissionServiceImpl::~ActorLoginPermissionServiceImpl() = default;

void ActorLoginPermissionServiceImpl::ListAllPermissions(
    ListPermissionsResult callback) {
  // TODO(crbug.com/491036780): Implement this.
  std::move(callback).Run({});
}

}  // namespace actor_login
