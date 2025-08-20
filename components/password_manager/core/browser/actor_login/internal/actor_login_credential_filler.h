// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_CREDENTIAL_FILLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_CREDENTIAL_FILLER_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/common/form_data.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
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

  // Reauthenticates the user before filling.
  void ReauthenticateAndFill(base::OnceClosure fill_form_cb);

  // Called after the reauthentication step with the result of the reauth
  // operation. Invokes `fill_form_cb` if authentication was successful.
  void OnDeviceReauthCompleted(base::OnceClosure fill_form_cb,
                               bool authenticated);

  // Sends a message to the renderer to fill the form in the `driver`'s frame,
  // identified by `form_renderer_id`. `username` and `password` are the
  // strings to fill in the form.
  // This method might be called async if reauthentication is needed beforehand.
  void FillForm(base::WeakPtr<password_manager::PasswordManagerDriver> driver,
                autofill::FormRendererId form_renderer_id,
                std::u16string username,
                std::u16string password);

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
