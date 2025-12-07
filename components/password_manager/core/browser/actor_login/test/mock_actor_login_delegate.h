// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_TEST_MOCK_ACTOR_LOGIN_DELEGATE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_TEST_MOCK_ACTOR_LOGIN_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/types/strong_alias.h"
#include "components/password_manager/core/browser/actor_login/actor_login_quality_logger_interface.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace actor_login {

class MockActorLoginDelegate : public ActorLoginDelegate {
 public:
  MockActorLoginDelegate();
  ~MockActorLoginDelegate() override;

  MOCK_METHOD(void,
              GetCredentials,
              (base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
               CredentialsOrErrorReply callback),
              (override));
  MOCK_METHOD(void,
              AttemptLogin,
              (const Credential& credential,
               bool should_store_permission,
               base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
               LoginStatusResultOrErrorReply callback),
              (override));
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_TEST_MOCK_ACTOR_LOGIN_DELEGATE_H_
