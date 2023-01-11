// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_OPERATION_CHAIN_RUNNER_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_OPERATION_CHAIN_RUNNER_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"

namespace ash {

class UserContext;

// This method allows to run a chain of `AuthOperationCallback`'s without
// creating intermediate methods.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC)
void RunOperationChain(std::unique_ptr<UserContext> context,
                       std::vector<AuthOperation> callbacks,
                       AuthSuccessCallback success_handler,
                       AuthErrorCallback error_handler);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_OPERATION_CHAIN_RUNNER_H_
