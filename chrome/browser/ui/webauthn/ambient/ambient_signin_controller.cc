// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/ambient/ambient_signin_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/webauthn/ambient/ambient_signin_bubble_view.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_type_flags.mojom.h"
#include "ui/views/widget/widget.h"

using blink::mojom::CredentialTypeFlags;
using content::RenderFrameHost;
using content::WebContents;

namespace ambient_signin {

AmbientSigninController::~AmbientSigninController() {
  if (model_) {
    model_->observers.RemoveObserver(this);
    model_ = nullptr;
  }
  if (ambient_signin_bubble_view_) {
    ambient_signin_bubble_view_->NotifyWidgetDestroyed();
    ambient_signin_bubble_view_ = nullptr;
  }
}

void AmbientSigninController::AddAndShowWebAuthnMethods(
    AuthenticatorRequestDialogModel* model,
    const std::vector<password_manager::PasskeyCredential>& credentials,
    int expected_credential_type_flags,
    PasskeyCredentialSelectionCallback callback) {
  CHECK(expected_credential_type_flags &
            static_cast<int>(CredentialTypeFlags::kPassword) ||
        expected_credential_type_flags &
            static_cast<int>(CredentialTypeFlags::kPublicKey));
  if (!model_) {
    model_ = model;
    model_->observers.AddObserver(this);
  } else {
    CHECK(model == model_);
  }

  passkey_selection_callback_ = std::move(callback);
  passkey_credentials_ = credentials;

  // TODO(358119268): There isn't a strong guarantee that passwords will ever
  // arrive here, since errors can be encountered on the credential manager
  // path. This needs to be addressed, and in general we need to better ensure
  // that state in both credential manager and webauthn code is adequately
  // cleaned up.
  if (credentials_received_state_ == CredentialsReceived::kPasskeys ||
      credentials_received_state_ ==
          CredentialsReceived::kPasswordsAndPasskeys) {
    return;
  }

  if (credentials_received_state_ == CredentialsReceived::kNone) {
    credentials_received_state_ = CredentialsReceived::kPasskeys;
    if (expected_credential_type_flags &
        static_cast<int>(CredentialTypeFlags::kPassword)) {
      // Wait for passwords.
      return;
    }
  } else {
    credentials_received_state_ = CredentialsReceived::kPasswordsAndPasskeys;
  }

  ShowBubble();
}

void AmbientSigninController::AddAndShowPasswordMethods(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> forms,
    int expected_credential_type_flags,
    password_manager::PasswordManagerClient::CredentialsCallback callback) {
  CHECK(expected_credential_type_flags &
            static_cast<int>(CredentialTypeFlags::kPassword) ||
        expected_credential_type_flags &
            static_cast<int>(CredentialTypeFlags::kPublicKey));
  password_selection_callback_ = std::move(callback);

  password_forms_.swap(forms);

  if (credentials_received_state_ == CredentialsReceived::kPasswords ||
      credentials_received_state_ ==
          CredentialsReceived::kPasswordsAndPasskeys) {
    return;
  }

  if (credentials_received_state_ == CredentialsReceived::kNone) {
    credentials_received_state_ = CredentialsReceived::kPasswords;
    if (expected_credential_type_flags &
        static_cast<int>(CredentialTypeFlags::kPublicKey)) {
      // Wait for passkeys.
      return;
    }
  } else {
    credentials_received_state_ = CredentialsReceived::kPasswordsAndPasskeys;
  }

  ShowBubble();
}

void AmbientSigninController::ShowBubble() {
  if (password_forms_.empty() && passkey_credentials_.empty()) {
    return;
  }

  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents ||
      web_contents->GetVisibility() == content::Visibility::HIDDEN) {
    return;
  }
  auto* browser = chrome::FindBrowserWithTab(web_contents);
  ToolbarButtonProvider* button_provider =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar_button_provider();
  auto* anchor_view = button_provider->GetAnchorView(std::nullopt);

  if (!ambient_signin_bubble_view_) {
    ambient_signin_bubble_view_ =
        new AmbientSigninBubbleView(anchor_view, this);
    ambient_signin_bubble_view_->ShowCredentials(passkey_credentials_,
                                                 password_forms_);
  }
}

AmbientSigninController::AmbientSigninController(
    RenderFrameHost* render_frame_host)
    : content::DocumentUserData<AmbientSigninController>(render_frame_host) {
  // TODO(crbug.com/358119268): This crashes if a request happens from a
  // WebContents that is not inside a tab.
  tabs::TabInterface* tab_interface_ = tabs::TabInterface::GetFromContents(
      WebContents::FromRenderFrameHost(render_frame_host));
  tab_subscriptions_.push_back(
      tab_interface_->RegisterWillEnterBackground(base::BindRepeating(
          &AmbientSigninController::TabWillEnterBackground, GetWeakPtr())));
  tab_subscriptions_.push_back(
      tab_interface_->RegisterDidEnterForeground(base::BindRepeating(
          &AmbientSigninController::TabDidEnterForeground, GetWeakPtr())));
}

void AmbientSigninController::OnPasskeySelected(
    const std::vector<uint8_t>& account_id) {
  std::move(passkey_selection_callback_).Run(account_id);
}

void AmbientSigninController::OnPasswordSelected(
    const password_manager::PasswordForm* form) {
  std::move(password_selection_callback_).Run(form);
}

std::u16string AmbientSigninController::GetRpId() const {
  return model_ ? base::UTF8ToUTF16(model_->relying_party_id)
                : std::u16string();
}

base::OnceClosure AmbientSigninController::GetSignInCallback() {
  CHECK(password_forms_.size() + passkey_credentials_.size() == 1);
  if (password_forms_.size()) {
    return base::BindOnce(&AmbientSigninController::OnPasswordSelected,
                          GetWeakPtr(), password_forms_.begin()->get());
  }
  return base::BindOnce(&AmbientSigninController::OnPasskeySelected,
                        GetWeakPtr(),
                        passkey_credentials_.begin()->credential_id());
}

void AmbientSigninController::OnWidgetDestroying(views::Widget* widget) {
  // The passkey callback does not have to be invoked because its state is
  // scoped to the request, but the password manager state is global and needs
  // to be resolved.
  if (password_selection_callback_) {
    std::move(password_selection_callback_).Run(nullptr);
  }
  ambient_signin_bubble_view_->NotifyWidgetDestroyed();
  ambient_signin_bubble_view_ = nullptr;
}

void AmbientSigninController::OnRequestComplete() {
  if (!ambient_signin_bubble_view_) {
    return;
  }
  ambient_signin_bubble_view_->Close();
}

void AmbientSigninController::OnModelDestroyed(
    AuthenticatorRequestDialogModel* model) {
  CHECK(model == model_);
  model_->observers.RemoveObserver(this);
  model_ = nullptr;
}

void AmbientSigninController::TabWillEnterBackground(
    tabs::TabInterface* tab_interface) {
  if (!ambient_signin_bubble_view_) {
    return;
  }
  ambient_signin_bubble_view_->Hide();
}

void AmbientSigninController::TabDidEnterForeground(
    tabs::TabInterface* tab_interface) {
  if (!ambient_signin_bubble_view_) {
    ShowBubble();
    return;
  }
  ambient_signin_bubble_view_->Show();
}

base::WeakPtr<AmbientSigninController> AmbientSigninController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

DOCUMENT_USER_DATA_KEY_IMPL(AmbientSigninController);

}  // namespace ambient_signin
