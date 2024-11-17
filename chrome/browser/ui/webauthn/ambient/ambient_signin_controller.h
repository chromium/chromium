// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_SIGNIN_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_SIGNIN_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/webauthn/ambient/ambient_signin_bubble_view.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/document_user_data.h"
#include "ui/views/widget/widget_observer.h"

struct AuthenticatorRequestDialogModel;

namespace base {
class CallbackListSubscription;
}  // namespace base

namespace content {
class RenderFrameHost;
}  // namespace content

namespace password_manager {
class PasskeyCredential;
struct PasswordForm;
}

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace ambient_signin {

// This class is responsible for displaying sign-in methods such as passkeys in
// a bubble like view over the document. Its lifetime is bound to the blink
// document that it is tied to. It will be gone when the RenderFrameHost is
// deleted.
// TODO(ambient): Move this class to c/b/ui/ambient and include other types of
// sign-in methods (e.g. FedCM)
class AmbientSigninController
    : public content::DocumentUserData<AmbientSigninController>,
      public AuthenticatorRequestDialogModel::Observer,
      public views::WidgetObserver {
 public:
  using PasskeyCredentialSelectionCallback =
      base::OnceCallback<void(const std::vector<uint8_t>)>;

  ~AmbientSigninController() override;

  // Adds and shows the WebAuthn credentials in the Ambient UI.
  void AddAndShowWebAuthnMethods(
      AuthenticatorRequestDialogModel* model,
      const std::vector<password_manager::PasskeyCredential>& credentials,
      int expected_credential_type_flags,
      PasskeyCredentialSelectionCallback callback);

  void AddAndShowPasswordMethods(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> forms,
      int expected_credential_type_flags,
      password_manager::PasswordManagerClient::CredentialsCallback callback);

  // Called when the user selects a passkey shown in the bubble.
  void OnPasskeySelected(const std::vector<uint8_t>& account_id);

  // Called when the user selects a password shown in the bubble.
  void OnPasswordSelected(const password_manager::PasswordForm* form);

  std::u16string GetRpId() const;
  base::OnceClosure GetSignInCallback();

  base::WeakPtr<AmbientSigninController> GetWeakPtr();

 private:
  // content::DocumentUserData<AmbientSigninController>:
  explicit AmbientSigninController(content::RenderFrameHost* render_frame_host);
  friend class content::DocumentUserData<AmbientSigninController>;
  DOCUMENT_USER_DATA_KEY_DECL();

  enum class CredentialsReceived {
    kNone,
    kPasskeys,
    kPasswords,
    kPasswordsAndPasskeys,
  };

  void ShowBubble();

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // AuthenticatorRequestDialogModel::Observer
  void OnRequestComplete() override;
  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override;

  // tabs::TabInterface related overrides:
  void TabWillEnterBackground(tabs::TabInterface* tab_interface);
  void TabDidEnterForeground(tabs::TabInterface* tab_interface);

  std::vector<base::CallbackListSubscription> tab_subscriptions_;
  raw_ptr<AmbientSigninBubbleView> ambient_signin_bubble_view_;
  PasskeyCredentialSelectionCallback passkey_selection_callback_;
  password_manager::PasswordManagerClient::CredentialsCallback
      password_selection_callback_;
  CredentialsReceived credentials_received_state_ = CredentialsReceived::kNone;
  std::vector<std::unique_ptr<password_manager::PasswordForm>> password_forms_;
  std::vector<password_manager::PasskeyCredential> passkey_credentials_;

  raw_ptr<AuthenticatorRequestDialogModel> model_;

  base::WeakPtrFactory<AmbientSigninController> weak_ptr_factory_{this};
};

}  // namespace ambient_signin

#endif  // CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_SIGNIN_CONTROLLER_H_
