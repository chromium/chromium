// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_CALLBACKS_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_CALLBACKS_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"

namespace ash {

class UserContext;

using AuthOperationCallback =
    base::OnceCallback<void(std::unique_ptr<UserContext>,
                            std::optional<AuthenticationError>)>;
using AuthOperation = base::OnceCallback<void(std::unique_ptr<UserContext>,
                                              AuthOperationCallback)>;
using AuthErrorCallback =
    base::OnceCallback<void(std::unique_ptr<UserContext>, AuthenticationError)>;
using AuthSuccessCallback =
    base::OnceCallback<void(std::unique_ptr<UserContext>)>;
using NoContextOperationCallback =
    base::OnceCallback<void(std::optional<AuthenticationError>)>;

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_CALLBACKS_H_
