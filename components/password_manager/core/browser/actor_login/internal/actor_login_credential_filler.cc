// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_credential_filler.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_interface.h"

namespace actor_login {

ActorLoginCredentialFiller::ActorLoginCredentialFiller(
    const url::Origin& origin,
    const Credential& credential,
    LoginStatusResultOrErrorReply callback)
    : origin_(origin),
      credential_(credential),
      callback_(std::move(callback)) {}

ActorLoginCredentialFiller::~ActorLoginCredentialFiller() = default;

void ActorLoginCredentialFiller::AttemptLogin(
    password_manager::PasswordManagerInterface* password_manager) {
  CHECK(password_manager);

  password_manager::PasswordFormCache* form_cache =
      password_manager->GetPasswordFormCache();
  CHECK(form_cache);
  bool has_signin_form = false;
  for (const auto& manager : form_cache->GetFormManagers()) {
    if (manager->GetDriver()->GetLastCommittedOrigin().IsSameOriginWith(
            origin_)) {
      const password_manager::PasswordForm* parsed_form =
          manager->GetParsedObservedForm();
      // TODO(crbug.com/427170499): Check if this is the right condition to
      // check for a signin form.
      if (parsed_form && parsed_form->IsLikelyLoginForm()) {
        has_signin_form = true;
        break;
      }
    }
  }

  if (!has_signin_form) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_),
                                  LoginStatusResult::kErrorNoSigninForm));
    return;
  }

  // TODO(crbug.com/427170499): Check if the credential matches a saved
  // credential and fill the form if it does.
}

}  // namespace actor_login
