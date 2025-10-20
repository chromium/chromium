// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_credential_filler.h"

#include <ranges>
#include <utility>

#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "base/functional/concurrent_closures.h"
#include "base/strings/to_string.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_form_finder.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace actor_login {

using autofill::FieldRendererId;
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

LoginStatusResult GetEndFillingResult(bool username_filled,
                                      bool password_filled) {
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

}  // namespace

ActorLoginCredentialFiller::ActorLoginCredentialFiller(
    const url::Origin& main_frame_origin,
    const Credential& credential,
    bool should_store_permission,
    PasswordManagerClient* client,
    LoginStatusResultOrErrorReply callback)
    : origin_(main_frame_origin),
      credential_(credential),
      should_store_permission_(should_store_permission),
      client_(client),
      login_form_finder_(std::make_unique<ActorLoginFormFinder>(client_)),
      callback_(std::move(callback)) {}

ActorLoginCredentialFiller::~ActorLoginCredentialFiller() = default;

void ActorLoginCredentialFiller::AttemptLogin(
    password_manager::PasswordManagerInterface* password_manager,
    const tabs::TabInterface& tab) {
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

  PasswordFormManager* signin_form_manager =
      login_form_finder_->GetSigninFormManager(origin_);
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

  base::OnceClosure fill_cb = base::DoNothing();
  if (base::FeatureList::IsEnabled(
          password_manager::features::kActorLoginFillingHeuristics)) {
    fill_cb = base::BindOnce(&ActorLoginCredentialFiller::FillAllEligibleFields,
                             weak_ptr_factory_.GetWeakPtr(),
                             stored_credential->username_value,
                             stored_credential->password_value);
  } else {
    if (should_store_permission_) {
      signin_form_manager->SetShouldStoreActorLoginPermission();
    }
    fill_cb = base::BindOnce(
        &ActorLoginCredentialFiller::FillForm, weak_ptr_factory_.GetWeakPtr(),
        driver, form_renderer_id, stored_credential->username_value,
        stored_credential->password_value);
  }

  if (client_->IsReauthBeforeFillingRequired(device_authenticator_.get())) {
    LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_WAITING_FOR_REAUTH);
    if (!tab.IsActivated()) {
      std::move(callback_).Run(LoginStatusResult::kErrorDeviceReauthRequired);
    } else {
      ReauthenticateAndFill(std::move(fill_cb));
    }
  } else {
    std::move(fill_cb).Run();
  }
}

const PasswordForm* ActorLoginCredentialFiller::GetMatchingStoredCredential(
    const PasswordFormManager& signin_form_manager) {
  const PasswordForm* matching_stored_credential = nullptr;
  for (const password_manager::PasswordForm& stored_credential_form :
       signin_form_manager.GetBestMatches()) {
    if (stored_credential_form.username_value == credential_.username &&
        ActorLoginFormFinder::GetSourceSiteOrAppFromUrl(
            stored_credential_form.url) == credential_.source_site_or_app) {
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
    std::move(callback_).Run(LoginStatusResult::kErrorDeviceReauthFailed);
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

  base::ConcurrentClosures concurrent_filling;
  FillField(driver.get(), form_to_fill->username_element_renderer_id, username,
            FieldType::kUsername, concurrent_filling.CreateClosure());
  FillField(driver.get(), form_to_fill->password_element_renderer_id, password,
            FieldType::kPassword, concurrent_filling.CreateClosure());

  std::move(concurrent_filling)
      .Done(base::BindOnce(&ActorLoginCredentialFiller::OnFillingDone,
                           weak_ptr_factory_.GetWeakPtr()));
}

void ActorLoginCredentialFiller::FillAllEligibleFields(
    std::u16string username,
    std::u16string password) {
  PasswordManagerInterface* password_manager = client_->GetPasswordManager();
  PasswordFormCache* form_cache = password_manager->GetPasswordFormCache();

  base::ConcurrentClosures concurrent_filling;
  for (const auto& manager : form_cache->GetFormManagers()) {
    if (!manager->GetDriver()) {
      continue;
    }
    if (!manager->GetDriver()->GetLastCommittedOrigin().IsSameOriginWith(
            origin_)) {
      continue;
    }

    const password_manager::PasswordForm* parsed_form =
        manager->GetParsedObservedForm();
    if (!parsed_form || !login_form_finder_->IsLoginForm(*parsed_form)) {
      continue;
    }
    if (should_store_permission_) {
      manager->SetShouldStoreActorLoginPermission();
    }

    FillField(manager->GetDriver().get(),
              parsed_form->username_element_renderer_id, username,
              FieldType::kUsername, concurrent_filling.CreateClosure());
    FillField(manager->GetDriver().get(),
              parsed_form->password_element_renderer_id, password,
              FieldType::kPassword, concurrent_filling.CreateClosure());
  }

  std::move(concurrent_filling)
      .Done(base::BindOnce(&ActorLoginCredentialFiller::OnFillingDone,
                           weak_ptr_factory_.GetWeakPtr()));
}

void ActorLoginCredentialFiller::FillField(PasswordManagerDriver* driver,
                                           FieldRendererId field_renderer_id,
                                           const std::u16string& value,
                                           FieldType type,
                                           base::OnceClosure closure) {
  if (field_renderer_id.is_null()) {
    ProcessSingleFillingResult(type, field_renderer_id, false);
    std::move(closure).Run();
    return;
  }
  driver->FillField(
      field_renderer_id, value,
      autofill::FieldPropertiesFlags::kAutofilledActorLogin,
      base::BindOnce(&ActorLoginCredentialFiller::ProcessSingleFillingResult,
                     weak_ptr_factory_.GetWeakPtr(), type, field_renderer_id)
          .Then(std::move(closure)));
}

void ActorLoginCredentialFiller::ProcessSingleFillingResult(
    FieldType field_type,
    autofill::FieldRendererId field_id,
    bool success) {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger =
      GetLogger(client_);
  LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_FILLING_FIELD_WITH_ID,
            base::ToString(field_id));
  switch (field_type) {
    case FieldType::kUsername: {
      LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_USERNAME_FILL_SUCCESS,
                base::ToString(success));
      username_filled_ = username_filled_ || success;
      break;
    }
    case FieldType::kPassword: {
      LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_PASSWORD_FILL_SUCCESS,
                base::ToString(success));
      password_filled_ = password_filled_ || success;
      break;
    }
  };
}

void ActorLoginCredentialFiller::OnFillingDone() {
  std::move(callback_).Run(
      GetEndFillingResult(username_filled_, password_filled_));
}

}  // namespace actor_login
