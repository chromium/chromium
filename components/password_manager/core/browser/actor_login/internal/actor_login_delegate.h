// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DELEGATE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"

namespace actor_login {

// Interface for the `ActorLoginDelegate`.
// This interface defines the core business logic delegated by
// `ActorLoginService`. It exists to facilitate mocking in tests.
class ActorLoginDelegate {
 public:
  virtual ~ActorLoginDelegate() = default;

  // Asynchronously retrieves credentials.
  virtual void GetCredentials(CredentialsOrErrorReply callback) = 0;

  // Attempts to log in using the provided `credential`.
  virtual void AttemptLogin(const Credential& credential,
                            LoginStatusResultOrErrorReply callback) = 0;
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DELEGATE_H_
