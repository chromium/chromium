// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_credential_filler.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

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

  CHECK(network::IsOriginPotentiallyTrustworthy(origin_));

  password_manager::PasswordFormCache* form_cache =
      password_manager->GetPasswordFormCache();
  CHECK(form_cache);
  password_manager::PasswordFormManager* signin_form_manager = nullptr;
  for (const auto& manager : form_cache->GetFormManagers()) {
    if (manager->GetDriver()->GetLastCommittedOrigin().IsSameOriginWith(
            origin_)) {
      const password_manager::PasswordForm* parsed_form =
          manager->GetParsedObservedForm();
      // TODO(crbug.com/427170499): Check if this is the right condition to
      // check for a signin form.
      if (parsed_form && parsed_form->IsLikelyLoginForm()) {
        signin_form_manager = manager.get();
        break;
      }
    }
  }

  if (!signin_form_manager) {
    std::move(callback_).Run(LoginStatusResult::kErrorNoSigninForm);
    return;
  }

  bool credential_is_saved_for_origin = false;
  for (const password_manager::PasswordForm& stored_credential_form :
       signin_form_manager->GetBestMatches()) {
    if (stored_credential_form.username_value == credential_.username &&
        GetSourceSiteOrAppFromUrl(stored_credential_form.url) ==
            credential_.source_site_or_app) {
      credential_is_saved_for_origin = true;
      break;
    }
  }

  if (!credential_is_saved_for_origin) {
    std::move(callback_).Run(LoginStatusResult::kErrorInvalidCredential);
    return;
  }

  // TODO(crbug.com/427170499): Fill the form.
}

}  // namespace actor_login
