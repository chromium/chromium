// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/request/token_based_auth_request.h"

#include "chromeos/ash/components/cryptohome/constants.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

TokenBasedAuthRequest::TokenBasedAuthRequest(
    AuthCompletionCallback on_auth_complete)
    : on_auth_complete_(std::move(on_auth_complete)) {}

TokenBasedAuthRequest::~TokenBasedAuthRequest() = default;

void TokenBasedAuthRequest::NotifyAuthSuccess(
    std::unique_ptr<UserContext> user_context) {
  CHECK(user_context);
  AuthProofToken token =
      AuthSessionStorage::Get()->Store(std::move(user_context));
  CHECK(on_auth_complete_);
  std::move(on_auth_complete_)
      .Run(true, token, cryptohome::kAuthsessionInitialLifetime);
}

void TokenBasedAuthRequest::NotifyAuthFailure() {
  CHECK(on_auth_complete_);
  std::move(on_auth_complete_)
      .Run(false, ash::AuthProofToken{}, base::TimeDelta{});
}

}  // namespace ash
