// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/password_credential_ui_controller.h"

#if !BUILDFLAG(IS_ANDROID)

#include <string>
#include <utility>

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

using content::GlobalRenderFrameHostId;
using content::RenderFrameHost;
using content::WebContents;

namespace {

std::u16string GetAuthenticationMessage(std::string_view rp_id) {
#if BUILDFLAG(IS_LINUX)
  return u"";
#else
  return l10n_util::GetStringFUTF16(IDS_PASSWORD_MANAGER_FILLING_REAUTH,
                                    base::UTF8ToUTF16(rp_id));
#endif  // BUILDFLAG(IS_LINUX)
}

}  // namespace

PasswordCredentialUIController::PasswordCredentialUIController(
    GlobalRenderFrameHostId render_frame_host_id,
    AuthenticatorRequestDialogModel* model)
    : render_frame_host_id_(render_frame_host_id) {
  model_observer_.Observe(model);
}

PasswordCredentialUIController::~PasswordCredentialUIController() = default;

password_manager::PasswordManagerClient*
PasswordCredentialUIController::GetPasswordManagerClient() const {
  if (client_for_testing_) {
    return client_for_testing_;
  }
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(GetRenderFrameHost());
  if (!web_contents) {
    return nullptr;
  }
  return ChromePasswordManagerClient::FromWebContents(web_contents);
}

bool PasswordCredentialUIController::IsAuthRequired() {
  // TODO(crbug.com/392549444): For the prototype, require screen lock only if
  // it's enabled (e.g. via PWM settings). This may change.
  auto* pwm_client = GetPasswordManagerClient();
  return pwm_client && pwm_client->GetPasswordFeatureManager()
                           ->IsBiometricAuthenticationBeforeFillingEnabled();
}

void PasswordCredentialUIController::SetPasswordSelectedCallback(
    content::AuthenticatorRequestClientDelegate::PasswordSelectedCallback
        callback) {
  password_selected_callback_ = callback;
}

void PasswordCredentialUIController::OnPasswordCredentialSelected(
    PasswordCredentialPair password) {
  if (!IsAuthRequired() ||
      model_observer_.GetSource()->local_auth_token.has_value()) {
    password_selected_callback_.Run(password_manager::CredentialInfo(
        password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
        password.first, password.first, GURL(), password.second,
        url::SchemeHostPort()));
    return;
  }
  filling_password_ = std::move(password);
  model_observer_.GetSource()->SetStep(
      AuthenticatorRequestDialogModel::Step::kPasswordOsAuth);
}

void PasswordCredentialUIController::OnStepTransition() {
  if (!model_observer_.GetSource()) {
    return;
  }

  if (model_observer_.GetSource()->step() ==
      AuthenticatorRequestDialogModel::Step::kPasswordOsAuth) {
    CHECK(filling_password_.has_value());
    auto manage_passwords_ui_controller = PasswordsModelDelegateFromWebContents(
        WebContents::FromRenderFrameHost(GetRenderFrameHost()));
    if (!manage_passwords_ui_controller) {
      return;
    }
    manage_passwords_ui_controller->AuthenticateUserWithMessage(
        GetAuthenticationMessage(model_observer_.GetSource()->relying_party_id),
        base::BindOnce(
            &PasswordCredentialUIController::OnAuthenticationCompleted,
            weak_ptr_factory_.GetWeakPtr(),
            std::move(filling_password_.value())));
    return;
  }
}

void PasswordCredentialUIController::OnModelDestroyed(
    AuthenticatorRequestDialogModel* model) {
  model_observer_.Reset();
}

void PasswordCredentialUIController::SetPasswordManagerClientForTesting(
    password_manager::PasswordManagerClient* client) {
  client_for_testing_ = client;
}

void PasswordCredentialUIController::OnAuthenticationCompleted(
    PasswordCredentialPair password,
    bool success) {
  if (!success) {
    model_observer_.GetSource()->CancelAuthenticatorRequest();
    return;
  }
  password_selected_callback_.Run(password_manager::CredentialInfo(
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
      password.first, password.first, GURL(), password.second,
      url::SchemeHostPort()));
}

RenderFrameHost* PasswordCredentialUIController::GetRenderFrameHost() const {
  RenderFrameHost* ret = RenderFrameHost::FromID(render_frame_host_id_);
  CHECK(ret);
  return ret;
}

#endif  // !BUILDFLAG(IS_ANDROID)
