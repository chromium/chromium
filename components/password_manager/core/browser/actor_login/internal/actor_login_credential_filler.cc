// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_credential_filler.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace actor_login {

using password_manager::PasswordForm;
using password_manager::PasswordFormCache;
using password_manager::PasswordFormManager;

ActorLoginCredentialFiller::ActorLoginCredentialFiller(
    const url::Origin& main_frame_origin,
    const Credential& credential,
    LoginStatusResultOrErrorReply callback)
    : origin_(main_frame_origin),
      credential_(credential),
      callback_(std::move(callback)) {}

ActorLoginCredentialFiller::~ActorLoginCredentialFiller() = default;

void ActorLoginCredentialFiller::AttemptLogin(
    password_manager::PasswordManagerInterface* password_manager) {
  CHECK(password_manager);

  CHECK(network::IsOriginPotentiallyTrustworthy(origin_));

  PasswordFormCache* form_cache = password_manager->GetPasswordFormCache();
  CHECK(form_cache);

  PasswordFormManager* signin_form_manager = nullptr;
  for (const auto& manager : form_cache->GetFormManagers()) {
    if (!manager->GetDriver()->GetLastCommittedOrigin().IsSameOriginWith(
            origin_)) {
      continue;
    }

    const PasswordForm* parsed_form = manager->GetParsedObservedForm();
    // TODO(crbug.com/427170499): Check if this is the right condition to
    // check for a signin form.
    if (!parsed_form || parsed_form->HasNewPasswordElement()) {
      continue;
    }

    // Prefer filling the primary main frame form if one exists.
    if (manager->GetDriver()->IsInPrimaryMainFrame()) {
      signin_form_manager = manager.get();
      break;
    }

    // Otherwise, store this form manager and look to see if there is a primary
    // main frame form later.
    if (!signin_form_manager) {
      signin_form_manager = manager.get();
    }
  }

  if (!signin_form_manager) {
    std::move(callback_).Run(LoginStatusResult::kErrorNoSigninForm);
    return;
  }

  const PasswordForm* stored_credential =
      GetMatchingStoredCredential(*signin_form_manager);

  if (!stored_credential) {
    std::move(callback_).Run(LoginStatusResult::kErrorInvalidCredential);
    return;
  }

  FillForm(*signin_form_manager, *stored_credential);
}

const PasswordForm* ActorLoginCredentialFiller::GetMatchingStoredCredential(
    const PasswordFormManager& signin_form_manager) {
  const PasswordForm* matching_stored_credential = nullptr;
  for (const password_manager::PasswordForm& stored_credential_form :
       signin_form_manager.GetBestMatches()) {
    if (stored_credential_form.username_value == credential_.username &&
        GetSourceSiteOrAppFromUrl(stored_credential_form.url) ==
            credential_.source_site_or_app) {
      matching_stored_credential = &stored_credential_form;
      break;
    }
  }
  return matching_stored_credential;
}

void ActorLoginCredentialFiller::FillForm(
    const PasswordFormManager& manager,
    const PasswordForm& stored_credential) {
  const password_manager::PasswordForm* form_to_fill =
      manager.GetParsedObservedForm();
  CHECK(form_to_fill);

  // TODO(crbug.com/427170499): Make sure the filling result is also based on
  // whether the renderer can actually fill.
  LoginStatusResult filling_result = LoginStatusResult::kErrorNoFillableFields;

  if (!form_to_fill->username_element_renderer_id.is_null()) {
    manager.GetDriver()->FillField(
        form_to_fill->username_element_renderer_id,
        stored_credential.username_value,
        autofill::FieldPropertiesFlags::kAutofilledActorLogin);
    filling_result = LoginStatusResult::kSuccessUsernameFilled;
  }

  if (!form_to_fill->password_element_renderer_id.is_null()) {
    manager.GetDriver()->FillField(
        form_to_fill->password_element_renderer_id,
        stored_credential.password_value,
        autofill::FieldPropertiesFlags::kAutofilledActorLogin);

    if (filling_result == LoginStatusResult::kSuccessUsernameFilled) {
      filling_result = LoginStatusResult::kSuccessUsernameAndPasswordFilled;
    } else {
      filling_result = LoginStatusResult::kSuccessPasswordFilled;
    }
  }

  std::move(callback_).Run(filling_result);
}

}  // namespace actor_login
