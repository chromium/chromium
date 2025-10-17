// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_CREDENTIAL_FILLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_CREDENTIAL_FILLER_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "url/gurl.h"

class ActorLoginFormFinder;

namespace password_manager {
class PasswordManagerInterface;
}  // namespace password_manager

namespace tabs {
class TabInterface;
}

namespace actor_login {

// Fills a given credential into the matching signin form if one exists.
class ActorLoginCredentialFiller {
 public:
  ActorLoginCredentialFiller(const url::Origin& main_frame_origin,
                             const Credential& credential,
                             bool should_store_permission,
                             password_manager::PasswordManagerClient* client,
                             LoginStatusResultOrErrorReply callback);
  ~ActorLoginCredentialFiller();

  ActorLoginCredentialFiller(const ActorLoginCredentialFiller&) = delete;
  ActorLoginCredentialFiller& operator=(const ActorLoginCredentialFiller&) =
      delete;

  // Attempts to fill the credential provided in the constructor.
  // `password_manager` is used to find the signin form.
  // `tab` is used if the user needs to re-authenticate. In this case the tab
  // must be in foreground, otherwise this will result in
  // `kErrorDeviceReauthRequired`.
  void AttemptLogin(
      password_manager::PasswordManagerInterface* password_manager,
      const tabs::TabInterface& tab);

 private:
  enum class FieldType { kUsername, kPassword };

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

  // Fills all eligible fields with `username` and `password`.
  void FillAllEligibleFields(std::u16string username, std::u16string password);

  // Fills the field of `type` identified by `field_renderer_id` within the
  // `driver`'s frame with `value`. `closure` will be called to signal
  // completion at the very end of the flow.
  void FillField(password_manager::PasswordManagerDriver* driver,
                 autofill::FieldRendererId field_renderer_id,
                 const std::u16string& value,
                 FieldType type,
                 base::OnceClosure closure);

  // Called with the success status of filling the respective field.
  void ProcessSingleFillingResult(FieldType field_type,
                                  autofill::FieldRendererId field_id,
                                  bool success);

  // Called when all filling operations have finished. Invokes `callback_`
  // with the result based on `username_filled_` and `password_filled_`.
  void OnFillingDone();

  // The origin of the primary main frame.
  const url::Origin origin_;

  // The credential to fill in either the primary main frame or the frame
  // matching the `origin_`.
  const Credential credential_;

  // Whether user chose to always allow actor login to use `credential_`
  const bool should_store_permission_ = false;

  // Populated with the aggregated results of the calls to fill.
  bool username_filled_ = false;
  bool password_filled_ = false;

  // Safe to access from everywhere apart from the destructor.
  raw_ptr<password_manager::PasswordManagerClient> client_ = nullptr;

  // Helper object for finding login forms.
  std::unique_ptr<ActorLoginFormFinder> login_form_finder_;

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
