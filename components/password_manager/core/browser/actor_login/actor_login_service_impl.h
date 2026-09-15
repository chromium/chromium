// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_SERVICE_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_SERVICE_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/actor_login/actor_login_quality_logger_interface.h"
#include "components/password_manager/core/browser/actor_login/actor_login_service.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_delegate.h"

namespace actor_login {

class ActorLoginDelegateClient;

class ActorLoginServiceImpl : public ActorLoginService {
 public:
  ActorLoginServiceImpl();
  ~ActorLoginServiceImpl() override;

  ActorLoginServiceImpl(const ActorLoginServiceImpl&) = delete;
  ActorLoginServiceImpl& operator=(const ActorLoginServiceImpl&) = delete;

  // `ActorLoginService` implementation:
  void GetCredentials(
      ActorLoginDelegateClient* client,
      bool has_sign_in_with_google_button,
      scoped_refptr<ActorLoginQualityLoggerInterface> mqls_logger,
      CredentialsOrErrorReply callback) override;
  void AttemptLogin(
      ActorLoginDelegateClient* client,
      const Credential& credential,
      bool should_store_permission,
      scoped_refptr<ActorLoginQualityLoggerInterface> mqls_logger,
      base::TimeTicks attempt_login_tool_start_time,
      FrameFillingStartedCallback frame_filling_started_cb,
      LoginStatusResultOrErrorReply done_callback,
      base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate) override;
  void SetActorLoginDelegateFactoryForTesting(
      base::RepeatingCallback<ActorLoginDelegate*(ActorLoginDelegateClient*)>
          factory);

 private:
  // Factory callback returning a new instance of `ActorLoginDelegate` or
  // an existing one if there is one already attached to the provided
  // `ActorLoginDelegateClient*`. Used to facilitate testing.
  base::RepeatingCallback<ActorLoginDelegate*(ActorLoginDelegateClient*)>
      actor_login_delegate_factory_;
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_SERVICE_IMPL_H_
