// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/actor_login_permission_service.h"

namespace actor_login {

FederatedPermission::FederatedPermission() = default;
FederatedPermission::FederatedPermission(const FederatedPermission&) = default;
FederatedPermission& FederatedPermission::operator=(
    const FederatedPermission&) = default;
FederatedPermission::~FederatedPermission() = default;

}  // namespace actor_login
