// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/actor_login_service_impl.h"

#include "base/functional/bind.h"
#include "base/types/expected.h"
#include "components/password_manager/core/browser/actor_login/actor_login_quality_logger_interface.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_delegate.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_delegate_impl.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_metrics.h"

namespace actor_login {

namespace {

ActorLoginDelegate* GetOrCreateDelegate(ActorLoginDelegateClient* client) {
  ActorLoginDelegateImpl* delegate =
      ActorLoginDelegateImpl::FromUserData(client);
  if (!delegate) {
    delegate = ActorLoginDelegateImpl::CreateForUserData(client);
  }
  return delegate;
}

void OnGetCredentialsResult(CredentialsOrErrorReply callback,
                            CredentialsOrError result) {
  RecordGetCredentialsResult(result);
  std::move(callback).Run(std::move(result));
}

void OnAttemptLoginResult(LoginStatusResultOrErrorReply done_callback,
                          LoginStatusResultOrError result) {
  RecordAttemptLoginResult(result);
  std::move(done_callback).Run(std::move(result));
}
}  // namespace

ActorLoginServiceImpl::ActorLoginServiceImpl() {
  actor_login_delegate_factory_ = base::BindRepeating(&GetOrCreateDelegate);
}

ActorLoginServiceImpl::~ActorLoginServiceImpl() = default;

void ActorLoginServiceImpl::GetCredentials(
    ActorLoginDelegateClient* client,
    bool has_sign_in_with_google_button,
    scoped_refptr<ActorLoginQualityLoggerInterface> mqls_logger,
    CredentialsOrErrorReply callback) {
  if (!client) {
    OnGetCredentialsResult(
        std::move(callback),
        base::unexpected(ActorLoginError::kInvalidTabInterface));
    return;
  }

  ActorLoginDelegate* delegate = actor_login_delegate_factory_.Run(client);

  delegate->GetCredentials(
      has_sign_in_with_google_button, mqls_logger,
      base::BindOnce(&OnGetCredentialsResult, std::move(callback)));
}

void ActorLoginServiceImpl::AttemptLogin(
    ActorLoginDelegateClient* client,
    const Credential& credential,
    bool should_store_permission,
    scoped_refptr<ActorLoginQualityLoggerInterface> mqls_logger,
    base::TimeTicks attempt_login_tool_start_time,
    FrameFillingStartedCallback frame_filling_started_cb,
    LoginStatusResultOrErrorReply done_callback,
    base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate) {
  if (!client) {
    OnAttemptLoginResult(
        std::move(done_callback),
        base::unexpected(ActorLoginError::kInvalidTabInterface));
    return;
  }

  ActorLoginDelegate* delegate = actor_login_delegate_factory_.Run(client);

  delegate->AttemptLogin(
      credential, should_store_permission, mqls_logger,
      attempt_login_tool_start_time, std::move(frame_filling_started_cb),
      base::BindOnce(&OnAttemptLoginResult, std::move(done_callback)),
      std::move(action_sequence_delegate));
}

void ActorLoginServiceImpl::SetActorLoginDelegateFactoryForTesting(
    base::RepeatingCallback<ActorLoginDelegate*(ActorLoginDelegateClient*)>
        factory) {
  actor_login_delegate_factory_ = factory;
}

}  // namespace actor_login
