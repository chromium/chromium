// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_CREDENTIAL_FILLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_CREDENTIAL_FILLER_H_

#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "url/gurl.h"

namespace password_manager {
class PasswordManagerInterface;
}  // namespace password_manager

namespace actor_login {

// Fills a given credential into the matching signin form if one exists.
class ActorLoginCredentialFiller {
 public:
  ActorLoginCredentialFiller(const url::Origin& main_frame_origin,
                             const Credential& credential,
                             LoginStatusResultOrErrorReply callback);
  ~ActorLoginCredentialFiller();

  ActorLoginCredentialFiller(const ActorLoginCredentialFiller&) = delete;
  ActorLoginCredentialFiller& operator=(const ActorLoginCredentialFiller&) =
      delete;

  // Attempts to fill the credential provided in the constructor.
  // `password_manager` is used to find the signin form.
  void AttemptLogin(
      password_manager::PasswordManagerInterface* password_manager);

 private:
  // The origin of the primary main frame.
  const url::Origin origin_;

  // The credential to fill in either the primary main frame or the frame
  // matching the `origin_`.
  const Credential credential_;

  // The callback to call with the result of the login attempt.
  LoginStatusResultOrErrorReply callback_;
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_CREDENTIAL_FILLER_H_
