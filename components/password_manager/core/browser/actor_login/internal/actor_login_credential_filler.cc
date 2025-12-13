// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_credential_filler.h"

#include <algorithm>
#include <ranges>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
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
#include "services/metrics/public/cpp/metrics_utils.h"
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
    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
    IsTaskInFocus is_task_in_focus,
    LoginStatusResultOrErrorReply callback)
    : origin_(main_frame_origin),
      credential_(credential),
      should_store_permission_(should_store_permission),
      client_(client),
      mqls_logger_(mqls_logger),
      start_time_(base::TimeTicks::Now()),
      login_form_finder_(std::make_unique<ActorLoginFormFinder>(client_)),
      is_task_in_focus_(std::move(is_task_in_focus)),
      callback_(std::move(callback)) {}

ActorLoginCredentialFiller::~ActorLoginCredentialFiller() {
  if (mqls_logger_) {
    mqls_logger_->AddAttemptLoginDetails(std::move(attempt_login_logs_));
  }
}

void ActorLoginCredentialFiller::AttemptLogin(
    password_manager::PasswordManagerInterface* password_manager) {
  CHECK(client_);
  CHECK(password_manager);

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger =
      GetLogger(client_);

  LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_FILLING_ATTEMPT_STARTED);

  CHECK(network::IsOriginPotentiallyTrustworthy(origin_));

  // The check is added separately in order to differentiate between having
  // no signin form on the page and filling being disallowed.
  if (!client_->IsFillingEnabled(origin_.GetURL())) {
    LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_FILLING_NOT_ALLOWED);
    // TODO(crbug.com/460687566): add kFillingNotAllowed to the proto and change
    // this outcome.
    BuildAttemptLoginOutcome(AttemptLoginOutcomeMqls::kUnspecified);
    std::move(callback_).Run(
        base::unexpected(ActorLoginError::kFillingNotAllowed));
    return;
  }

  // Disallow filling a credential requested for a different primary main frame
  // origin than the one it was requested for.
  if (!origin_.IsSameOriginWith(credential_.request_origin)) {
    LogStatus(logger.get(),
              Logger::STRING_ACTOR_LOGIN_PRIMARY_MAIN_FRAME_ORIGIN_CHANGED);
    BuildAttemptLoginOutcome(AttemptLoginOutcomeMqls::kInvalidCredential);
    std::move(callback_).Run(LoginStatusResult::kErrorInvalidCredential);
    return;
  }

  CHECK(network::IsOriginPotentiallyTrustworthy(origin_));

  FetchEligibleForms(
      base::BindOnce(&ActorLoginCredentialFiller::ProcessRetrievedForms,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ActorLoginCredentialFiller::FetchEligibleForms(
    base::OnceCallback<
        void(std::vector<password_manager::PasswordFormManager*>)>
        on_forms_retrieved_cb) {
  auto log_parsed_forms_details =
      [](base::WeakPtr<ActorLoginCredentialFiller> filler,
         FormFinderResult form_finder_result) {
        if (filler) {
          for (auto& detail : form_finder_result.parsed_forms_details) {
            *filler->attempt_login_logs_.add_parsed_form_details() =
                std::move(detail);
          }
        }
        return std::move(form_finder_result.eligible_managers);
      };

  if (base::FeatureList::IsEnabled(
          password_manager::features::kActorLoginFieldVisibilityCheck)) {
    login_form_finder_->GetEligibleLoginFormManagersAsync(
        origin_,
        base::BindOnce(log_parsed_forms_details, weak_ptr_factory_.GetWeakPtr())
            .Then(std::move(on_forms_retrieved_cb)));
  } else {
    std::move(on_forms_retrieved_cb)
        .Run(log_parsed_forms_details(
            weak_ptr_factory_.GetWeakPtr(),
            login_form_finder_->GetEligibleLoginFormManagers(origin_)));
  }
}

void ActorLoginCredentialFiller::ProcessRetrievedForms(
    std::vector<password_manager::PasswordFormManager*> eligible_managers) {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger =
      GetLogger(client_);

  password_manager::PasswordFormManager* signin_form_manager =
      ActorLoginFormFinder::GetSigninFormManager(eligible_managers);

  if (!signin_form_manager) {
    LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_NO_SIGNIN_FORM);
    BuildAttemptLoginOutcome(AttemptLoginOutcomeMqls::kNoSignInForm);
    std::move(callback_).Run(LoginStatusResult::kErrorNoSigninForm);
    return;
  }

  const PasswordForm* stored_credential =
      GetMatchingStoredCredential(*signin_form_manager);

  if (!stored_credential) {
    LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_INVALID_CREDENTIAL);
    BuildAttemptLoginOutcome(AttemptLoginOutcomeMqls::kInvalidCredential);
    std::move(callback_).Run(LoginStatusResult::kErrorInvalidCredential);
    return;
  }

  device_authenticator_ = client_->GetDeviceAuthenticator();

  MaybeReauthAndFillAllEligibleFields(std::move(eligible_managers),
                                      *stored_credential);
}

void ActorLoginCredentialFiller::MaybeReauthAndFillAllEligibleFields(
    std::vector<password_manager::PasswordFormManager*> eligible_managers,
    const password_manager::PasswordForm& stored_credential) {
  // If there is a login form in the primary main frame, don't fill
  // iframes as we prefer forms from the primary main frame.
  bool is_primary_main_frame =
      ActorLoginFormFinder::GetSigninFormManager(eligible_managers)
          ->GetDriver()
          ->IsInPrimaryMainFrame();

  // TODO(crbug.com/458711310): Avoid re-calling this method after fetching
  // forms if re-authentication occurs before filling.
  if (client_->IsReauthBeforeFillingRequired(device_authenticator_.get())) {
    base::OnceCallback<void(
        std::vector<password_manager::PasswordFormManager*>)>
        fill_all_fields_cb =
            base::BindOnce(&ActorLoginCredentialFiller::FillAllEligibleFields,
                           weak_ptr_factory_.GetWeakPtr(), stored_credential,
                           is_primary_main_frame);

    AttemptReauth(base::BindOnce(
        &ActorLoginCredentialFiller::FetchEligibleForms,
        weak_ptr_factory_.GetWeakPtr(), std::move(fill_all_fields_cb)));
    return;
  }

  FillAllEligibleFields(stored_credential, is_primary_main_frame,
                        std::move(eligible_managers));
}

void ActorLoginCredentialFiller::AttemptReauth(base::OnceClosure on_reauth_cb) {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger =
      GetLogger(client_);
  LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_WAITING_FOR_REAUTH);

  if (base::FeatureList::IsEnabled(
          password_manager::features::kActorLoginReauthTaskRefocus) &&
      !is_task_in_focus_.Run()) {
    BuildAttemptLoginOutcome(AttemptLoginOutcomeMqls::kReauthRequired);
    std::move(callback_).Run(LoginStatusResult::kErrorDeviceReauthRequired);
    return;
  }

  ReauthenticateAndFill(std::move(on_reauth_cb));
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

  reauth_start_time_ = base::TimeTicks::Now();
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
    BuildAttemptLoginOutcome(AttemptLoginOutcomeMqls::kReauthFailed);
    std::move(callback_).Run(LoginStatusResult::kErrorDeviceReauthFailed);
    return;
  }

  std::move(fill_form_cb).Run();
}

void ActorLoginCredentialFiller::FillAllEligibleFields(
    const password_manager::PasswordForm& stored_credential,
    bool should_skip_iframes,
    std::vector<password_manager::PasswordFormManager*> eligible_managers) {
  if (reauth_start_time_.has_value()) {
    base::TimeDelta reauth_duration =
        base::TimeTicks::Now() - reauth_start_time_.value();
    start_time_ += reauth_duration;
  }
  base::ConcurrentClosures concurrent_filling;
  if (should_skip_iframes) {
    std::erase_if(eligible_managers,
                  [](const PasswordFormManager* form_manager) {
                    return !form_manager->GetDriver()->IsInPrimaryMainFrame();
                  });
  }

  for (PasswordFormManager* manager : eligible_managers) {
    bool stored_credential_belongs_to_manager = std::ranges::any_of(
        manager->GetBestMatches().begin(), manager->GetBestMatches().end(),
        [&stored_credential](const PasswordForm& best_match) {
          return password_manager::ArePasswordFormUniqueKeysEqual(
              stored_credential, best_match);
        });
    if (base::FeatureList::IsEnabled(
            password_manager::features::kActorLoginSameSiteIframeSupport) &&
        !stored_credential_belongs_to_manager) {
      continue;
    }
    if (should_store_permission_) {
      manager->SetShouldStoreActorLoginPermission();
    }

    const password_manager::PasswordForm* parsed_form =
        manager->GetParsedObservedForm();
    autofill::FormGlobalId form_global_id = parsed_form->form_data.global_id();
    ActorLoginFormFinder::SetFormData(
        *filling_results_[form_global_id].mutable_form_data(), *parsed_form);

    FillField(manager->GetDriver().get(), form_global_id,
              parsed_form->username_element_renderer_id,
              stored_credential.username_value, FieldType::kUsername,
              concurrent_filling.CreateClosure());
    FillField(manager->GetDriver().get(), form_global_id,
              parsed_form->password_element_renderer_id,
              stored_credential.password_value, FieldType::kPassword,
              concurrent_filling.CreateClosure());
  }

  std::move(concurrent_filling)
      .Done(base::BindOnce(&ActorLoginCredentialFiller::OnFillingDone,
                           weak_ptr_factory_.GetWeakPtr()));
}

void ActorLoginCredentialFiller::FillField(
    PasswordManagerDriver* driver,
    autofill::FormGlobalId form_global_id,
    FieldRendererId field_renderer_id,
    const std::u16string& value,
    FieldType type,
    base::OnceClosure closure) {
  if (field_renderer_id.is_null()) {
    ProcessSingleFillingResult(form_global_id, type, field_renderer_id, false);
    std::move(closure).Run();
    return;
  }
  driver->FillField(
      field_renderer_id, value,
      autofill::FieldPropertiesFlags::kAutofilledActorLogin,
      base::BindOnce(&ActorLoginCredentialFiller::ProcessSingleFillingResult,
                     weak_ptr_factory_.GetWeakPtr(), form_global_id, type,
                     field_renderer_id)
          .Then(std::move(closure)));
}

void ActorLoginCredentialFiller::ProcessSingleFillingResult(
    autofill::FormGlobalId form_id,
    FieldType field_type,
    FieldRendererId field_id,
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
      if (!field_id.is_null()) {
        filling_results_[form_id].set_was_username_filled(success);
      }
      break;
    }
    case FieldType::kPassword: {
      LogStatus(logger.get(), Logger::STRING_ACTOR_LOGIN_PASSWORD_FILL_SUCCESS,
                base::ToString(success));
      password_filled_ = password_filled_ || success;
      if (!field_id.is_null()) {
        filling_results_[form_id].set_was_password_filled(success);
      }
      break;
    }
  };
}

void ActorLoginCredentialFiller::BuildAttemptLoginOutcome(
    AttemptLoginOutcomeMqls outcome) {
  base::TimeDelta request_duration = base::TimeTicks::Now() - start_time_;
  attempt_login_logs_.set_attempt_login_time_ms(
      ukm::GetSemanticBucketMinForDurationTiming(
          request_duration.InMilliseconds()));
  attempt_login_logs_.set_outcome(OutcomeEnumToProtoType(outcome));

  for (const auto& [_, filling_result] : filling_results_) {
    attempt_login_logs_.add_filling_form_result()->CopyFrom(filling_result);
  }
}

void ActorLoginCredentialFiller::OnFillingDone() {
  LoginStatusResult result =
      GetEndFillingResult(username_filled_, password_filled_);
  if (result == LoginStatusResult::kErrorNoFillableFields) {
    BuildAttemptLoginOutcome(AttemptLoginOutcomeMqls::kNoFillableFields);
  } else {
    BuildAttemptLoginOutcome(AttemptLoginOutcomeMqls::kSuccess);
  }
  std::move(callback_).Run(result);
}

}  // namespace actor_login
