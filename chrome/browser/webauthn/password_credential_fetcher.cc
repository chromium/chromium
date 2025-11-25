// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/password_credential_fetcher.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

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

}  // namespace

std::unique_ptr<PasswordCredentialFetcher> PasswordCredentialFetcher::Create(
    RenderFrameHost* rfh) {
  if (instance_for_testing_) {
    return base::WrapUnique(instance_for_testing_);
  }
  return base::WrapUnique(new PasswordCredentialFetcher(rfh));
}

std::unique_ptr<PasswordCredentialFetcher>
PasswordCredentialFetcher::CreateForTesting(
    RenderFrameHost* rfh,
    std::unique_ptr<password_manager::FormFetcher> form_fetcher,
    password_manager::PasswordManagerClient* client) {
  auto fetcher = base::WrapUnique(new PasswordCredentialFetcher(rfh));
  fetcher->form_fetcher_ = std::move(form_fetcher);
  fetcher->pwm_client_for_testing_ = client;
  return fetcher;
}

PasswordCredentialFetcher::~PasswordCredentialFetcher() = default;

void PasswordCredentialFetcher::FetchPasswords(
    const GURL& url,
    PasswordCredentialsReceivedCallback callback) {
  CHECK(!callback_);
  callback_ = std::move(callback);

  CreateFormFetcher(url);
  form_fetcher_->Fetch();
  form_fetcher_->AddConsumer(this);
}

void PasswordCredentialFetcher::UpdateDateLastUsed(
    const std::u16string& username,
    const std::u16string& password) {
  CHECK(form_fetcher_);

  password_manager::PasswordManagerClient* client = GetPasswordManagerClient();
  if (!client) {
    return;
  }

  for (const auto& match : form_fetcher_->GetBestMatches()) {
    if (match.username_value == username && match.password_value == password) {
      PasswordForm updated_form = match;
      updated_form.date_last_used = base::Time::Now();
      if ((updated_form.in_store &
           password_manager::PasswordForm::Store::kProfileStore) !=
          password_manager::PasswordForm::Store::kNotSet) {
        if (auto* store = client->GetProfilePasswordStore()) {
          store->UpdateLogin(updated_form);
        }
      }
      if ((updated_form.in_store &
           password_manager::PasswordForm::Store::kAccountStore) !=
          password_manager::PasswordForm::Store::kNotSet) {
        if (auto* store = client->GetAccountPasswordStore()) {
          store->UpdateLogin(updated_form);
        }
      }
    }
  }
}

void PasswordCredentialFetcher::SetInstanceForTesting(
    PasswordCredentialFetcher* instance) {
  instance_for_testing_ = instance;
}

PasswordCredentialFetcher::PasswordCredentialFetcher(RenderFrameHost* rfh)
    : rfh_(rfh) {}

void PasswordCredentialFetcher::OnFetchCompleted() {
  CHECK(callback_);
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

void PasswordCredentialFetcher::CreateFormFetcher(const GURL& url) {
  if (form_fetcher_) {
    return;
  }
  form_fetcher_ = std::make_unique<password_manager::FormFetcherImpl>(
      GetSynthesizedFormForUrl(url), GetPasswordManagerClient(),
      /*should_migrate_http_passwords=*/false);
}

password_manager::PasswordManagerClient*
PasswordCredentialFetcher::GetPasswordManagerClient() const {
  if (pwm_client_for_testing_) {
    return pwm_client_for_testing_;
  }
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfh_);

  if (!web_contents) {
    return nullptr;
  }

  return ChromePasswordManagerClient::FromWebContents(web_contents);
}

PasswordCredentialFetcher* PasswordCredentialFetcher::instance_for_testing_ =
    nullptr;
