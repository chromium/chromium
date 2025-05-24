// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/password_credential_controller.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

using content::GlobalRenderFrameHostId;
using content::RenderFrameHost;
using content::WebContents;
using password_manager::PasswordForm;
using password_manager::PasswordFormDigest;

namespace {

PasswordFormDigest GetSynthesizedFormForUrl(GURL url) {
  PasswordFormDigest digest{PasswordForm::Scheme::kHtml, std::string(), url};
  digest.signon_realm = digest.url.spec();
  return digest;
}

password_manager::PasswordManagerClient* GetPasswordManagerClient(
    RenderFrameHost& render_frame_host) {
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host);

  if (!web_contents) {
    return nullptr;
  }

  return ChromePasswordManagerClient::FromWebContents(web_contents);
}

std::u16string GetAuthenticationMessage(std::string_view rp_id) {
#if BUILDFLAG(IS_LINUX)
  return u"";
#else
  return l10n_util::GetStringFUTF16(IDS_PASSWORD_MANAGER_FILLING_REAUTH,
                                    base::UTF8ToUTF16(rp_id));
#endif  // BUILDFLAG(IS_LINUX)
}

}  // namespace

PasswordCredentialController::PasswordCredentialController(
    GlobalRenderFrameHostId render_frame_host_id,
    AuthenticatorRequestDialogModel* model)
    : render_frame_host_id_(render_frame_host_id), model_(model) {
  model_observer_.Observe(model_);
}

PasswordCredentialController::~PasswordCredentialController() = default;

void PasswordCredentialController::FetchPasswords(
    const GURL& url,
    PasswordCredentialsReceivedCallback callback) {
  callback_ = std::move(callback);
  form_fetcher_ = GetFormFetcher(url);
  form_fetcher_->Fetch();
  form_fetcher_->AddConsumer(this);
}

bool PasswordCredentialController::IsAuthRequired() {
  // TODO(crbug.com/392549444): For the prototype, require screen lock only if
  // it's enabled (e.g. via PWM settings). This may change.
  auto* pwm_client = GetPasswordManagerClient(*GetRenderFrameHost());
  return pwm_client && pwm_client->GetPasswordFeatureManager()
                           ->IsBiometricAuthenticationBeforeFillingEnabled();
}

void PasswordCredentialController::SetPasswordSelectedCallback(
    content::AuthenticatorRequestClientDelegate::PasswordSelectedCallback
        callback) {
  password_selected_callback_ = callback;
}

void PasswordCredentialController::OnPasswordCredentialSelected(
    PasswordCredentialPair password) {
  if (!IsAuthRequired() || model_->local_auth_token.has_value()) {
    password_selected_callback_.Run(password_manager::CredentialInfo(
        password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
        password.first, password.first, GURL(), password.second,
        url::SchemeHostPort()));
    return;
  }
  filling_password_ = std::move(password);
  model_->SetStep(AuthenticatorRequestDialogModel::Step::kPasswordOsAuth);
}

void PasswordCredentialController::OnStepTransition() {
  if (model_->step() ==
      AuthenticatorRequestDialogModel::Step::kPasswordOsAuth) {
    CHECK(filling_password_.has_value());
    auto manage_passwords_ui_controller = PasswordsModelDelegateFromWebContents(
        WebContents::FromRenderFrameHost(GetRenderFrameHost()));
    if (!manage_passwords_ui_controller) {
      return;
    }
    manage_passwords_ui_controller->AuthenticateUserWithMessage(
        GetAuthenticationMessage(model_->relying_party_id),
        base::BindOnce(&PasswordCredentialController::OnAuthenticationCompleted,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(filling_password_.value())));
    return;
  }
}

void PasswordCredentialController::OnFetchCompleted() {
  CHECK(!callback_.is_null());
  PasswordCredentials credentials;
  std::ranges::transform(form_fetcher_->GetBestMatches(),
                         std::back_inserter(credentials),
                         [](const PasswordForm& form) {
                           return std::make_unique<PasswordForm>(form);
                         });
  std::erase_if(credentials, [](const std::unique_ptr<PasswordForm>& form) {
    return form->IsFederatedCredential() || form->username_value.empty();
  });
  std::move(callback_).Run(std::move(credentials));
}

std::unique_ptr<password_manager::FormFetcher>
PasswordCredentialController::GetFormFetcher(const GURL& url) {
  return std::make_unique<password_manager::FormFetcherImpl>(
      GetSynthesizedFormForUrl(url),
      GetPasswordManagerClient(*GetRenderFrameHost()),
      /*should_migrate_http_passwords=*/false);
}

RenderFrameHost* PasswordCredentialController::GetRenderFrameHost() const {
  RenderFrameHost* ret = RenderFrameHost::FromID(render_frame_host_id_);
  CHECK(ret);
  return ret;
}

void PasswordCredentialController::OnAuthenticationCompleted(
    PasswordCredentialPair password,
    bool success) {
  if (!success) {
    model_->CancelAuthenticatorRequest();
    return;
  }
  password_selected_callback_.Run(password_manager::CredentialInfo(
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
      password.first, password.first, GURL(), password.second,
      url::SchemeHostPort()));
}
