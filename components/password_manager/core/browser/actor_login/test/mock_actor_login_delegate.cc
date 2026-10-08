// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/test/mock_actor_login_delegate.h"

#include <utility>

namespace actor_login {

MockActorLoginDelegate::MockActorLoginDelegate() = default;
MockActorLoginDelegate::~MockActorLoginDelegate() = default;

void MockActorLoginDelegate::AttemptLogin(
    const Credential& credential,
    bool should_store_permission,
    scoped_refptr<ActorLoginQualityLoggerInterface> mqls_logger,
    base::TimeTicks attempt_login_tool_start_time,
    FrameFillingStartedCallback frame_filling_started_cb,
    LoginStatusResultOrErrorReply done_callback,
    base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate) {
  AttemptLogin(credential, should_store_permission, std::move(mqls_logger),
               std::move(action_sequence_delegate),
               std::move(frame_filling_started_cb), std::move(done_callback),
               attempt_login_tool_start_time);
}

}  // namespace actor_login
