// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/actor_login/actor_login_quality_logger_interface.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
namespace actor_login {

class ActorLoginDelegateClient;

// Interface for the `ActorLoginService`.
// This service provides methods for retrieving credentials and attempting
// login. It exists to facilitate mocking in tests.
class ActorLoginService {
 public:
  virtual ~ActorLoginService() = default;

  // Asynchronously retrieves credentials for the given `client`.
  // The `mqls_logger` is owned by the caller to ensure the same instance is
  // used to log both `GetCredentials` and `AttemptLogin`.
  // The `callback` will
  // be invoked with a `base::expected` containing either a list of
  // `Credential`s or an `ActorLoginError`.
  virtual void GetCredentials(
      ActorLoginDelegateClient* client,
      bool has_sign_in_with_google_button,
      scoped_refptr<ActorLoginQualityLoggerInterface> mqls_logger,
      CredentialsOrErrorReply callback) = 0;

  // Attempts to log in using the provided `credential` for the given `client`.
  // If `should_store_permission` is true, `credential` will be updated to store
  // the permission to use it in actor login.
  // The `mqls_logger` is owned by the caller to ensure the same instance is
  // used to log both `GetCredentials` and `AttemptLogin`.
  // The `done_callback` will be invoked with a `base::expected` containing
  // either a `LoginStatusResult` or an `ActorLoginError`.
  // The `action_sequence_delegate` allows for communicating outcomes of a login
  // when additional steps are involved after AttemptLogin has completed.
  virtual void AttemptLogin(
      ActorLoginDelegateClient* client,
      const Credential& credential,
      bool should_store_permission,
      scoped_refptr<ActorLoginQualityLoggerInterface> mqls_logger,
      base::TimeTicks attempt_login_tool_start_time,
      FrameFillingStartedCallback frame_filling_started_cb,
      LoginStatusResultOrErrorReply done_callback,
      base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate) = 0;
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_SERVICE_H_
