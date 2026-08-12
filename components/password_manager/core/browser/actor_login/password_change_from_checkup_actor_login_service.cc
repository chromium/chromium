// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/password_change_from_checkup_actor_login_service.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/concurrent_closures.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/core/browser/actor_login/automated_password_change_credential_filler.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_delegate_client.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_form_finder.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/url_formatter/elide_url.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "url/origin.h"

namespace actor_login {

PasswordChangeFromCheckupActorLoginService::
    PasswordChangeFromCheckupActorLoginService(
        password_manager::StoredCredential credential)
    : credential_(std::move(credential)) {}

PasswordChangeFromCheckupActorLoginService::
    ~PasswordChangeFromCheckupActorLoginService() = default;

// TODO(crbug.com/509823221): Actor Login MQLS logs should be uploaded and
// recorded as APC flows. Currently, MQLS is disabled for Actor Login
// when `kPasswordChangeWithGlic` is enabled. Instead, the MQLS logs
// should be marked as APC flows and uploaded.
void PasswordChangeFromCheckupActorLoginService::GetCredentials(
    ActorLoginDelegateClient* client,
    bool has_sign_in_with_google_button,
    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
    CredentialsOrErrorReply callback) {
  CredentialsOrErrorReply async_callback =
      base::BindPostTaskToCurrentDefault(std::move(callback));
  if (!client) {
    // TODO(crbug.com/511976430): Update metrics for Actor Login.
    std::move(async_callback)
        .Run(base::unexpected(ActorLoginError::kInvalidTabInterface));
    return;
  }
  url::Origin request_origin = client->GetLastCommittedOriginForMainFrame();
  if (!net::SchemefulSite::IsSameSite(credential_.url,
                                      request_origin.GetURL())) {
    // TODO(crbug.com/511976430): Update metrics for Actor Login.
    std::move(async_callback)
        .Run(base::unexpected(ActorLoginError::kFillingNotAllowed));
    return;
  }

  password_manager::PasswordManagerClient* password_manager_client =
      client->GetPasswordManagerClient();
  if (!password_manager_client) {
    // TODO(crbug.com/511976430): Update metrics for Actor Login.
    std::move(async_callback)
        .Run(base::unexpected(ActorLoginError::kInvalidTabInterface));
    return;
  }

  login_form_finder_ =
      std::make_unique<ActorLoginFormFinder>(password_manager_client);

  login_form_finder_->GetEligibleLoginFormManagersAsync(
      request_origin,
      base::BindOnce(&PasswordChangeFromCheckupActorLoginService::
                         OnEligibleLoginFormManagersRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), request_origin,
                     std::move(async_callback)));
}

void PasswordChangeFromCheckupActorLoginService::
    OnEligibleLoginFormManagersRetrieved(url::Origin request_origin,
                                         CredentialsOrErrorReply callback,
                                         FormFinderResult form_finder_result) {
  bool has_signin_form = ActorLoginFormFinder::GetSigninFormManager(
                             form_finder_result.eligible_managers) != nullptr;

  std::vector<Credential> credentials;
  Credential credential;
  credential.id = Credential::Id(1);
  credential.username = credential_.username_value;
  credential.source_site_or_app =
      actor_login::ActorLoginFormFinder::GetSourceSiteOrAppFromUrl(
          credential_.url);
  credential.request_origin = request_origin;
  credential.display_origin = url_formatter::FormatOriginForSecurityDisplay(
      credential.request_origin,
      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  credential.type = CredentialType::kPassword;
  credential.immediatelyAvailableToLogin = has_signin_form;
  // For the automated password change use case, the permission
  // is given at the moment the user agrees to start the flow.
  // This marks the permission as persistent so it is automatically
  // selected and does not trigger the credential picker.
  // It should not be saved to store, and just be available for this flow.
  credential.has_persistent_permission = true;
  credentials.push_back(credential);

  std::move(callback).Run(std::move(credentials));
}

void PasswordChangeFromCheckupActorLoginService::AttemptLogin(
    ActorLoginDelegateClient* client,
    const Credential& credential,
    bool should_store_permission,
    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
    base::TimeTicks attempt_login_tool_start_time,
    FrameFillingStartedCallback frame_filling_started_cb,
    LoginStatusResultOrErrorReply done_callback,
    base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate) {
  LoginStatusResultOrErrorReply async_callback =
      base::BindPostTaskToCurrentDefault(std::move(done_callback));
  if (!client) {
    std::move(async_callback)
        .Run(base::unexpected(ActorLoginError::kInvalidTabInterface));
    return;
  }
  CHECK(credential.username == credential_.username_value);

  url::Origin request_origin = client->GetLastCommittedOriginForMainFrame();
  if (!net::SchemefulSite::IsSameSite(credential_.url,
                                      request_origin.GetURL())) {
    // TODO(crbug.com/511976430): Update metrics for Actor Login.
    std::move(async_callback).Run(LoginStatusResult::kErrorNoSigninForm);
    return;
  }

  password_manager::PasswordManagerClient* password_manager_client =
      client->GetPasswordManagerClient();
  if (!password_manager_client) {
    // TODO(crbug.com/511976430): Update metrics for Actor Login.
    std::move(async_callback)
        .Run(base::unexpected(ActorLoginError::kInvalidTabInterface));
    return;
  }
  password_manager::PasswordManagerInterface* password_manager =
      password_manager_client->GetPasswordManager();

  if (!password_manager) {
    // TODO(crbug.com/511976430): Update metrics for Actor Login.
    std::move(async_callback).Run(LoginStatusResult::kErrorNoSigninForm);
    return;
  }

  // TODO(crbug.com/509823221): Actor Login MQLS logs should be uploaded and
  // recorded as APC flows. Currently, MQLS is disabled for Actor Login
  // when `kPasswordChangeWithGlic` is enabled. Instead, the MQLS logs
  // should be marked as APC flows and uploaded.
  credential_filler_ =
      std::make_unique<AutomatedPasswordChangeCredentialFiller>(
          request_origin, credential, password_manager_client,
          /*mqls_logger=*/nullptr, attempt_login_tool_start_time,
          base::NullCallback(), std::move(async_callback),
          password_manager::CloneStoredCredential(credential_));

  credential_filler_->AttemptLogin(password_manager);
}

}  // namespace actor_login
