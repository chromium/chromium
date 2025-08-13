// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_CREDENTIAL_FILLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_CREDENTIAL_FILLER_H_

#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
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
  // Retrieves the full data of a saved credential for the form managed
  // by `signin_form_manager` corresponding to `credential_`.
  const password_manager::PasswordForm* GetMatchingStoredCredential(
      const password_manager::PasswordFormManager& signin_form_manager);

  // Sends a message to the renderer to fill the form associated with the
  // `manager` with the contents of `stored_credential`.
  void FillForm(const password_manager::PasswordFormManager& manager,
                const password_manager::PasswordForm& stored_credential);

  // Called with the success status of filling the respective field.
  // Once both methods have been invoked, the result is passed on via
  // `callback_`.
  void OnUsernameFillingDone(bool success);
  void OnPasswordFillingDone(bool success);

  // The origin of the primary main frame.
  const url::Origin origin_;

  // The credential to fill in either the primary main frame or the frame
  // matching the `origin_`.
  const Credential credential_;

  // Populated once the request to fill the field comes back with a success
  // reply from the renderer.
  std::optional<bool> username_filled_;
  std::optional<bool> password_filled_;

  // The callback to call with the result of the login attempt.
  LoginStatusResultOrErrorReply callback_;

  // Used to reauthenticate the user before filling
  // the credential.
  std::unique_ptr<device_reauth::DeviceAuthenticator> device_authenticator_;

  // Member variables should appear before the WeakPtrFactory, to ensure
  // that any WeakPtrs to `ActorLoginCredentialFiller` are invalidated before
  // its member variables' destructors are executed, rendering them invalid.
  base::WeakPtrFactory<ActorLoginCredentialFiller> weak_ptr_factory_{this};
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_CREDENTIAL_FILLER_H_
