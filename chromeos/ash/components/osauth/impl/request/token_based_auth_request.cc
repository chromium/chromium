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

void TokenBasedAuthRequest::CompleteAuthAttempt(
    std::unique_ptr<UserContext> user_context,
    bool success) {
  CHECK(on_auth_complete_);
  if (success) {
    CHECK(user_context);  // A user_context should always be present on logical
                          // success.

    AuthProofToken token{};
    base::TimeDelta timeout = cryptohome::kAuthsessionInitialLifetime;

    // Only generate a real token if we have a UserContext from a successful
    // authentication that established a cryptohome session (i.e., has a
    // non-null lifetime). Otherwise, for logical success without a real auth
    // session, no token is generated.
    if (!user_context->GetSessionLifetime().is_null()) {
      token = AuthSessionStorage::Get()->Store(std::move(user_context));
    }
    std::move(on_auth_complete_).Run(true, token, timeout);
    return;
  }
  std::move(on_auth_complete_)
      .Run(false, ash::AuthProofToken{}, base::TimeDelta{});
}

void TokenBasedAuthRequest::NotifyAuthResult(
    std::unique_ptr<UserContext> user_context,
    AuthResult result) {
  CompleteAuthAttempt(std::move(user_context), result == AuthResult::kSuccess);
}

}  // namespace ash
