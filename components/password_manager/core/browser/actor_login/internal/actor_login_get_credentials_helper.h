// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_GET_CREDENTIALS_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_GET_CREDENTIALS_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {
class PasswordManagerClient;
class PasswordManagerInterface;
}

namespace actor_login {

// Helper class to get credentials for the Actor Login feature.
class ActorLoginGetCredentialsHelper
    : public password_manager::FormFetcher::Consumer {
 public:
  ActorLoginGetCredentialsHelper(
      const url::Origin& origin,
      password_manager::PasswordManagerClient* client,
      password_manager::PasswordManagerInterface* password_manager,
      CredentialsOrErrorReply callback);

  ActorLoginGetCredentialsHelper(const ActorLoginGetCredentialsHelper&) =
      delete;
  ActorLoginGetCredentialsHelper& operator=(
      const ActorLoginGetCredentialsHelper&) = delete;

  ~ActorLoginGetCredentialsHelper() override;

 private:
  // password_manager::FormFetcher::Consumer:
  void OnFetchCompleted() override;

  url::Origin request_origin_;
  CredentialsOrErrorReply callback_;
  raw_ptr<password_manager::PasswordManagerInterface> password_manager_ =
      nullptr;

  std::unique_ptr<password_manager::FormFetcher> owned_form_fetcher_;
  // The form fetcher from which credentials will be retrieved. If a
  // `PasswordFormManager` for a sign-in form already exists, this will be a
  // non-owning pointer to its `FormFetcher`. Otherwise, this class will own the
  // `FormFetcher` via `owned_form_fetcher_`.
  raw_ptr<password_manager::FormFetcher> form_fetcher_ = nullptr;
  bool immediately_available_to_login_ = false;
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_GET_CREDENTIALS_HELPER_H_
