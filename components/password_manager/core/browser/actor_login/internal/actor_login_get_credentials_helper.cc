// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_get_credentials_helper.h"

#include <ranges>

#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_util.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "url/origin.h"

namespace actor_login {

namespace {

Credential PasswordFormToCredential(
    url::Origin request_origin,
    const password_manager::PasswordForm& form) {
  CHECK(form.match_type);
  CHECK_NE(form.match_type.value(),
           password_manager::PasswordForm::MatchType::kGrouped);
  Credential credential;
  credential.username = form.username_value;
  credential.source_site_or_app = GetSourceSiteOrAppFromUrl(form.url);
  credential.request_origin = request_origin;
  // TODO(crbug.com/434165685): Use PasswordManager to set the real value here.
  credential.immediatelyAvailableToLogin = true;
  return credential;
}

}  // namespace

ActorLoginGetCredentialsHelper::ActorLoginGetCredentialsHelper(
    const url::Origin& origin,
    password_manager::PasswordManagerClient* client,
    CredentialsOrErrorReply callback)
    : request_origin_(origin), callback_(std::move(callback)) {
  password_manager::PasswordFormDigest form_digest(
      password_manager::PasswordForm::Scheme::kHtml,
      password_manager_util::GetSignonRealm(origin.GetURL()), origin.GetURL());
  form_fetcher_ = std::make_unique<password_manager::FormFetcherImpl>(
      std::move(form_digest), client,
      /*should_migrate_http_passwords=*/false);
  form_fetcher_->Fetch();
  form_fetcher_->AddConsumer(this);
}

ActorLoginGetCredentialsHelper::~ActorLoginGetCredentialsHelper() = default;

void ActorLoginGetCredentialsHelper::OnFetchCompleted() {
  std::vector<Credential> result;
  std::ranges::transform(
      form_fetcher_->GetBestMatches(), std::back_inserter(result),
      [&](const password_manager::PasswordForm& form) -> Credential {
        return PasswordFormToCredential(request_origin_, form);
      });
  std::move(callback_).Run(std::move(result));
}

}  // namespace actor_login
