// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_PASSWORD_CHANGE_FROM_CHECKUP_ACTOR_LOGIN_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_PASSWORD_CHANGE_FROM_CHECKUP_ACTOR_LOGIN_SERVICE_H_

#include <string>

#include "components/password_manager/core/browser/actor_login/actor_login_service.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace actor_login {

class AutomatedPasswordChangeCredentialFiller;
class ActorLoginFormFinder;
struct FormFinderResult;

// Implements the `ActorLoginService` interface for the automated password
// change flow.
class PasswordChangeFromCheckupActorLoginService : public ActorLoginService {
 public:
  explicit PasswordChangeFromCheckupActorLoginService(
      password_manager::StoredCredential credential);
  ~PasswordChangeFromCheckupActorLoginService() override;

  PasswordChangeFromCheckupActorLoginService(
      const PasswordChangeFromCheckupActorLoginService&) = delete;
  PasswordChangeFromCheckupActorLoginService& operator=(
      const PasswordChangeFromCheckupActorLoginService&) = delete;

  // ActorLoginService
  void GetCredentials(
      ActorLoginDelegateClient* client,
      bool has_sign_in_with_google_button,
      base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
      CredentialsOrErrorReply callback) override;

  void AttemptLogin(
      ActorLoginDelegateClient* client,
      const Credential& credential,
      bool should_store_permission,
      base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
      base::TimeTicks attempt_login_tool_start_time,
      FrameFillingStartedCallback frame_filling_started_cb,
      LoginStatusResultOrErrorReply done_callback,
      base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate) override;

 private:
  void OnEligibleLoginFormManagersRetrieved(
      url::Origin request_origin,
      CredentialsOrErrorReply callback,
      FormFinderResult form_finder_result);

  password_manager::StoredCredential credential_;
  std::unique_ptr<ActorLoginFormFinder> login_form_finder_;
  std::unique_ptr<AutomatedPasswordChangeCredentialFiller> credential_filler_;
  base::WeakPtrFactory<PasswordChangeFromCheckupActorLoginService>
      weak_ptr_factory_{this};
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_PASSWORD_CHANGE_FROM_CHECKUP_ACTOR_LOGIN_SERVICE_H_
