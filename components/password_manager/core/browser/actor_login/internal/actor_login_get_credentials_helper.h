// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_GET_CREDENTIALS_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_GET_CREDENTIALS_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "url/gurl.h"

namespace password_manager {
class PasswordManagerClient;
}

namespace actor_login {

// Helper class to get credentials for the Actor Login feature.
class ActorLoginGetCredentialsHelper
    : public password_manager::FormFetcher::Consumer {
 public:
  ActorLoginGetCredentialsHelper(
      const GURL& url,
      password_manager::PasswordManagerClient* client,
      CredentialsOrErrorReply callback);

  ActorLoginGetCredentialsHelper(const ActorLoginGetCredentialsHelper&) =
      delete;
  ActorLoginGetCredentialsHelper& operator=(
      const ActorLoginGetCredentialsHelper&) = delete;

  ~ActorLoginGetCredentialsHelper() override;

 private:
  // password_manager::FormFetcher::Consumer:
  void OnFetchCompleted() override;

  CredentialsOrErrorReply callback_;
  std::unique_ptr<password_manager::FormFetcher> form_fetcher_;
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_GET_CREDENTIALS_HELPER_H_
