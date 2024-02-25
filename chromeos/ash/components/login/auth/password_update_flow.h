// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PASSWORD_UPDATE_FLOW_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PASSWORD_UPDATE_FLOW_H_

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"

namespace ash {

class UserContext;

// Executes a chain of operations that update the user's configured password
// auth factor from the old to the new value.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH) PasswordUpdateFlow {
 public:
  PasswordUpdateFlow();
  PasswordUpdateFlow(const PasswordUpdateFlow&) = delete;
  PasswordUpdateFlow& operator=(const PasswordUpdateFlow&) = delete;
  ~PasswordUpdateFlow();

  // Starts the auth factor update operations.
  // `user_context` should contain the new credential; it should have no
  // AuthSessionId. On completion, either `success_callback` or `error_callback`
  // is called.
  void Start(std::unique_ptr<UserContext> user_context,
             const std::string& old_password,
             AuthSuccessCallback success_callback,
             AuthErrorCallback error_callback);

 private:
  void ContinueWithAuthSession(const std::string& old_password,
                               AuthSuccessCallback success_callback,
                               AuthErrorCallback error_callback,
                               bool user_exists,
                               std::unique_ptr<UserContext> user_context,
                               std::optional<AuthenticationError> error);

  AuthPerformer auth_performer_;
  AuthFactorEditor auth_factor_editor_;
  base::WeakPtrFactory<PasswordUpdateFlow> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PASSWORD_UPDATE_FLOW_H_
