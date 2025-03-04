// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/password_credential_controller_impl.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

using content::RenderFrameHost;

namespace webauthn {

namespace {

PasswordFormDigest GetSynthesizedFormForUrl(GURL url) {
  PasswordFormDigest digest{PasswordForm::Scheme::kHtml, std::string(), url};
  digest.signon_realm = digest.url.spec();
  return digest;
}

password_manager::PasswordManagerClient* GetPasswordManagerClient(
    RenderFrameHost& render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host);

  if (!web_contents) {
    return nullptr;
  }

  return ChromePasswordManagerClient::FromWebContents(web_contents);
}

}  // namespace

PasswordCredentialControllerImpl::PasswordCredentialControllerImpl(
    GlobalRenderFrameHostId render_frame_host_id,
    AuthenticatorRequestDialogModel* model)
    : render_frame_host_id_(render_frame_host_id), model_(model) {
  model_observer_.Observe(model_.get());
}

PasswordCredentialControllerImpl::~PasswordCredentialControllerImpl() = default;

void PasswordCredentialControllerImpl::FetchPasswords(
    const GURL& url,
    PasswordCredentialsReceivedCallback callback) {
  callback_ = std::move(callback);
  form_fetcher_ = GetFormFetcher(url);
  form_fetcher_->Fetch();
  form_fetcher_->AddConsumer(this);
}

bool PasswordCredentialControllerImpl::IsAuthRequired() {
  // TODO(crbug.com/392549444): For the prototype, require screen lock only if
  // it's enabled (e.g. via PWM settings). This may change.
  auto* pwm_client = GetPasswordManagerClient(*GetRenderFrameHost());
  return pwm_client && pwm_client->GetPasswordFeatureManager()
                           ->IsBiometricAuthenticationBeforeFillingEnabled();
}

void PasswordCredentialControllerImpl::SetPasswordSelectedCallback(
    AuthenticatorRequestClientDelegate::PasswordSelectedCallback callback) {
  password_selected_callback_ = callback;
}

void PasswordCredentialControllerImpl::OnPasswordCredentialSelected(
    PasswordPair password) {
  // TODO(crbug.com/392549444): Consider adding screen lock auth, etc. for
  // password selection. For prototyping this should be alright.

  password_selected_callback_.Run(password_manager::CredentialInfo(
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
      std::get<0>(password), std::get<0>(password), GURL(),
      std::get<1>(password), url::SchemeHostPort()));
}

void PasswordCredentialControllerImpl::OnFetchCompleted() {
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
PasswordCredentialControllerImpl::GetFormFetcher(const GURL& url) {
  return std::make_unique<password_manager::FormFetcherImpl>(
      GetSynthesizedFormForUrl(url),
      GetPasswordManagerClient(*GetRenderFrameHost()),
      /*should_migrate_http_passwords=*/false);
}

RenderFrameHost* PasswordCredentialControllerImpl::GetRenderFrameHost() const {
  RenderFrameHost* ret =
      content::RenderFrameHost::FromID(render_frame_host_id_);
  CHECK(ret);
  return ret;
}

}  // namespace webauthn
