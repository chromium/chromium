// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_credential_filler.h"

#include <ranges>
#include <utility>

#include "base/functional/callback.h"
#include "base/strings/to_string.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_util.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/strings/grit/components_strings.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace actor_login {

using autofill::SavePasswordProgressLogger;
using password_manager::BrowserSavePasswordProgressLogger;
using password_manager::PasswordForm;
using password_manager::PasswordFormCache;
using password_manager::PasswordFormManager;
using password_manager::PasswordManagerClient;
using password_manager::PasswordManagerDriver;
using password_manager::PasswordManagerInterface;

using Logger = autofill::SavePasswordProgressLogger;

namespace {

std::unique_ptr<BrowserSavePasswordProgressLogger> GetLogger(
    PasswordManagerClient* client) {
  autofill::LogManager* log_manager = client->GetCurrentLogManager();
  if (log_manager && log_manager->IsLoggingActive()) {
    return std::make_unique<BrowserSavePasswordProgressLogger>(log_manager);
  }

  return nullptr;
}

void LogStatus(BrowserSavePasswordProgressLogger* logger,
               SavePasswordProgressLogger::StringID label,
               const std::string& value = "") {
  if (!logger) {
    return;
  }
  if (value.empty()) {
    logger->LogMessage(label);
  } else {
    logger->LogString(label, value);
  }
}

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
    PasswordManagerClient* client,
    LoginStatusResultOrErrorReply callback)
    : origin_(main_frame_origin),
      credential_(credential),
      client_(client),
      callback_(std::move(callback)) {}

ActorLoginCredentialFiller::~ActorLoginCredentialFiller() = default;

void ActorLoginCredentialFiller::AttemptLogin(
    password_manager::PasswordManagerInterface* password_manager) {
  CHECK(client_);
  CHECK(password_manager);

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger =
      GetLogger(client_);

  LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_FILLING_ATTEMPT_STARTED);

  // AttemptLogin wouldn't fill even without this check, because
  // `PasswordFormManager` isn't created if this returns false. However, if we
  // don't add the check here, the error message returned to the caller would be
  // "kErrorNoSigninForm", which would be inaccurate.
  if (!client_->IsFillingEnabled(origin_.GetURL())) {
    LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_FILLING_NOT_ALLOWED);
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

    // Prefer filling the primary main frame form if one exists, but
    // also prefer more recently-parsed forms.
    if (manager->GetDriver()->IsInPrimaryMainFrame()) {
      signin_form_manager = manager.get();
    }

    // Otherwise, store this form manager and look to see if there is a primary
    // main frame form later.
    if (!signin_form_manager) {
      signin_form_manager = manager.get();
    }
  }

  if (!signin_form_manager) {
    LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_NO_SIGNIN_FORM);
    std::move(callback_).Run(LoginStatusResult::kErrorNoSigninForm);
    return;
  }

  const PasswordForm* stored_credential =
      GetMatchingStoredCredential(*signin_form_manager);

  if (!stored_credential) {
    LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_INVALID_CREDENTIAL);
    std::move(callback_).Run(LoginStatusResult::kErrorInvalidCredential);
    return;
  }

  device_authenticator_ = client_->GetDeviceAuthenticator();

  base::WeakPtr<PasswordManagerDriver> driver =
      signin_form_manager->GetDriver()->AsWeakPtr();
  autofill::FormRendererId form_renderer_id =
      signin_form_manager->GetParsedObservedForm()->form_data.renderer_id();

  auto fill_form_cb = base::BindOnce(
      &ActorLoginCredentialFiller::FillForm, weak_ptr_factory_.GetWeakPtr(),
      driver, form_renderer_id, stored_credential->username_value,
      stored_credential->password_value);

  if (client_->IsReauthBeforeFillingRequired(device_authenticator_.get())) {
    LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_WAITING_FOR_REAUTH);
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
    std::unique_ptr<BrowserSavePasswordProgressLogger> logger =
        GetLogger(client_);
    LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_REAUTH_FAILED);
    std::move(callback_).Run(LoginStatusResult::kErrorFillingNotAllowed);
    return;
  }

  std::move(fill_form_cb).Run();
}

void ActorLoginCredentialFiller::FillForm(
    base::WeakPtr<PasswordManagerDriver> driver,
    autofill::FormRendererId form_renderer_id,
    std::u16string username,
    std::u16string password) {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger =
      GetLogger(client_);
  if (!driver) {
    LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_FRAME_CHANGED);
    std::move(callback_).Run(LoginStatusResult::kErrorNoFillableFields);
    return;
  }

  PasswordManagerInterface* password_manager = driver->GetPasswordManager();
  PasswordFormCache* form_cache = password_manager->GetPasswordFormCache();
  const PasswordForm* form_to_fill =
      form_cache->GetPasswordForm(driver.get(), form_renderer_id);

  if (!form_to_fill) {
    LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_FORM_WENT_AWAY);
    std::move(callback_).Run(LoginStatusResult::kErrorNoFillableFields);
    return;
  }

  if (form_to_fill->username_element_renderer_id.is_null()) {
    LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_NO_USERNAME_FIELD);
    OnUsernameFillingDone(false);
  } else {
    driver->FillField(
        form_to_fill->username_element_renderer_id, username,
        autofill::FieldPropertiesFlags::kAutofilledActorLogin,
        base::BindOnce(&ActorLoginCredentialFiller::OnUsernameFillingDone,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (form_to_fill->password_element_renderer_id.is_null()) {
    LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_NO_PASWORD_FIELD);
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
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger =
      GetLogger(client_);
  LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_USERNAME_FILL_SUCCESS,
            base::ToString(success));
  if (!password_filled_.has_value()) {
    return;
  }
  std::move(callback_).Run(
      GetFillingResult(username_filled_.value(), password_filled_.value()));
}

void ActorLoginCredentialFiller::OnPasswordFillingDone(bool success) {
  password_filled_ = success;
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger =
      GetLogger(client_);
  LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_PASSWORD_FILL_SUCCESS,
            base::ToString(success));
  if (!username_filled_.has_value()) {
    return;
  }
  std::move(callback_).Run(
      GetFillingResult(username_filled_.value(), password_filled_.value()));
}

}  // namespace actor_login
