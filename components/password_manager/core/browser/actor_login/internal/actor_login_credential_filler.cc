// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_credential_filler.h"

#include <ranges>
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
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/strings/grit/components_strings.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace actor_login {

using password_manager::PasswordForm;
using password_manager::PasswordFormCache;
using password_manager::PasswordFormManager;
using password_manager::PasswordManagerDriver;
using password_manager::PasswordManagerInterface;

namespace {

LoginStatusResult GetFillingResult(bool username_filled, bool password_filled) {
  if (username_filled && password_filled) {
    return LoginStatusResult::kSuccessUsernameAndPasswordFilled;
  }

  if (username_filled) {
    return LoginStatusResult::kSuccessUsernameFilled;
  }

  if (password_filled) {
    return LoginStatusResult::kSuccessPasswordFilled;
  }

  return LoginStatusResult::kErrorNoFillableFields;
}

bool IsElementFocusable(autofill::FieldRendererId renderer_id,
                        const autofill::FormData& form_data) {
  auto field = std::ranges::find(form_data.fields(), renderer_id,
                                 &autofill::FormFieldData::renderer_id);
  CHECK(field != form_data.fields().end());
  return field->is_focusable();
}

bool IsLoginForm(const password_manager::PasswordForm& form) {
  const bool has_focusable_username =
      form.HasUsernameElement() &&
      IsElementFocusable(form.username_element_renderer_id, form.form_data);
  const bool has_focusable_password =
      form.HasPasswordElement() &&
      IsElementFocusable(form.password_element_renderer_id, form.form_data);
  const bool has_focusable_new_password =
      form.HasNewPasswordElement() &&
      IsElementFocusable(form.new_password_element_renderer_id, form.form_data);

  return (has_focusable_username || has_focusable_password) &&
         !has_focusable_new_password;
}

}  // namespace

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

  password_manager::PasswordManagerClient* client =
      password_manager->GetClient();
  CHECK(client);
  // AttemptLogin wouldn't fill even without this check, because
  // `PasswordFormManager` isn't created if this returns false. However, if we
  // don't add the check here, the error message returned to the caller would be
  // "kErrorNoSigninForm", which would be inaccurate.
  if (!client->IsFillingEnabled(origin_.GetURL())) {
    std::move(callback_).Run(LoginStatusResult::kErrorFillingNotAllowed);
    return;
  }

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
    if (!parsed_form || !IsLoginForm(*parsed_form)) {
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

  device_authenticator_ = client->GetDeviceAuthenticator();

  base::WeakPtr<PasswordManagerDriver> driver =
      signin_form_manager->GetDriver()->AsWeakPtr();
  autofill::FormRendererId form_renderer_id =
      signin_form_manager->GetParsedObservedForm()->form_data.renderer_id();

  auto fill_form_cb = base::BindOnce(
      &ActorLoginCredentialFiller::FillForm, weak_ptr_factory_.GetWeakPtr(),
      driver, form_renderer_id, stored_credential->username_value,
      stored_credential->password_value);

  if (client->IsReauthBeforeFillingRequired(device_authenticator_.get())) {
    ReauthenticateAndFill(std::move(fill_form_cb));
  } else {
    std::move(fill_form_cb).Run();
  }
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

void ActorLoginCredentialFiller::ReauthenticateAndFill(
    base::OnceClosure fill_form_cb) {
  std::u16string message;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  const std::u16string origin =
      base::UTF8ToUTF16(password_manager::GetShownOrigin(origin_));
  message =
      l10n_util::GetStringFUTF16(IDS_PASSWORD_MANAGER_FILLING_REAUTH, origin);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  auto on_reauth_completed =
      base::BindOnce(&ActorLoginCredentialFiller::OnDeviceReauthCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(fill_form_cb));

  device_authenticator_->AuthenticateWithMessage(
      message, password_manager::metrics_util::TimeCallbackMediumTimes(
                   std::move(on_reauth_completed),
                   "PasswordManager.ActorLogin.AuthenticationTime2"));
}

void ActorLoginCredentialFiller::OnDeviceReauthCompleted(
    base::OnceClosure fill_form_cb,
    bool authenticated) {
  if (!authenticated) {
    std::move(callback_).Run(LoginStatusResult::kErrorFillingNotAllowed);
    return;
  }

  std::move(fill_form_cb).Run();
}

void ActorLoginCredentialFiller::FillForm(
    base::WeakPtr<password_manager::PasswordManagerDriver> driver,
    autofill::FormRendererId form_renderer_id,
    std::u16string username,
    std::u16string password) {
  if (!driver) {
    std::move(callback_).Run(LoginStatusResult::kErrorNoFillableFields);
    return;
  }

  PasswordManagerInterface* password_manager = driver->GetPasswordManager();
  PasswordFormCache* form_cache = password_manager->GetPasswordFormCache();
  const PasswordForm* form_to_fill =
      form_cache->GetPasswordForm(driver.get(), form_renderer_id);

  if (!form_to_fill) {
    std::move(callback_).Run(LoginStatusResult::kErrorNoFillableFields);
    return;
  }

  if (form_to_fill->username_element_renderer_id.is_null()) {
    OnUsernameFillingDone(false);
  } else {
    driver->FillField(
        form_to_fill->username_element_renderer_id, username,
        autofill::FieldPropertiesFlags::kAutofilledActorLogin,
        base::BindOnce(&ActorLoginCredentialFiller::OnUsernameFillingDone,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (form_to_fill->password_element_renderer_id.is_null()) {
    OnPasswordFillingDone(false);
  } else {
    driver->FillField(
        form_to_fill->password_element_renderer_id, password,
        autofill::FieldPropertiesFlags::kAutofilledActorLogin,
        base::BindOnce(&ActorLoginCredentialFiller::OnPasswordFillingDone,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ActorLoginCredentialFiller::OnUsernameFillingDone(bool success) {
  username_filled_ = success;
  if (!password_filled_.has_value()) {
    return;
  }
  std::move(callback_).Run(
      GetFillingResult(username_filled_.value(), password_filled_.value()));
}

void ActorLoginCredentialFiller::OnPasswordFillingDone(bool success) {
  password_filled_ = success;
  if (!username_filled_.has_value()) {
    return;
  }
  std::move(callback_).Run(
      GetFillingResult(username_filled_.value(), password_filled_.value()));
}

}  // namespace actor_login
