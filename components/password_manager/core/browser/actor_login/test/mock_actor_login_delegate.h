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
              (bool has_sign_in_with_google_button,
               scoped_refptr<ActorLoginQualityLoggerInterface> mqls_logger,
               CredentialsOrErrorReply callback),
              (override));
  void AttemptLogin(
      const Credential& credential,
      bool should_store_permission,
      scoped_refptr<ActorLoginQualityLoggerInterface> mqls_logger,
      base::TimeTicks attempt_login_tool_start_time,
      FrameFillingStartedCallback frame_filling_started_cb,
      LoginStatusResultOrErrorReply done_callback,
      base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate) override;

  // Workaround for an LLVM x86 32-bit backend miscompilation (with PGO) where
  // SelectionDAG's MergeConsecutiveStores merges the 8-byte `base::TimeTicks`
  // and two 4-byte `base::OnceCallback` `byval` stack arguments into a single
  // 16-byte load (`s128`) whose MachineMemOperand only references the first
  // `byval` argument, causing MachineScheduler to reorder the `OnceCallback`
  // move-constructor zero-stores ahead of the 16-byte load. Placing
  // `action_sequence_delegate` (which has an out-of-line move constructor)
  // before the callbacks and passing `attempt_login_tool_start_time` by const
  // reference prevents the 16-byte load merge.
  // TODO(https://crbug.com/571177792): Remove this when PGO LLVM issue is fixed
  // by reverting crrev.com/c/8532034.
  MOCK_METHOD(void,
              AttemptLogin,
              (const Credential& credential,
               bool should_store_permission,
               scoped_refptr<ActorLoginQualityLoggerInterface> mqls_logger,
               base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate,
               FrameFillingStartedCallback frame_filling_started_cb,
               LoginStatusResultOrErrorReply done_callback,
               const base::TimeTicks& attempt_login_tool_start_time));
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_TEST_MOCK_ACTOR_LOGIN_DELEGATE_H_
