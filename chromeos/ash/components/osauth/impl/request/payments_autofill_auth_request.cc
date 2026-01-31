// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/request/payments_autofill_auth_request.h"

#include <string>
#include <utility>

#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/impl/request/token_based_auth_request.h"
#include "chromeos/ash/components/osauth/public/request/auth_request.h"

namespace ash {

PaymentsAutofillAuthRequest::PaymentsAutofillAuthRequest(
    const std::u16string& prompt,
    TokenBasedAuthRequest::AuthCompletionCallback on_auth_complete)
    : TokenBasedAuthRequest(std::move(on_auth_complete)), prompt_(prompt) {}

PaymentsAutofillAuthRequest::~PaymentsAutofillAuthRequest() = default;

void PaymentsAutofillAuthRequest::NotifyAuthResult(
    std::unique_ptr<UserContext> user_context,
    AuthRequest::AuthResult result) {
  if (result == AuthRequest::AuthResult::kAuthNotAvailable) {
    // For payments autofill, if no auth factors are available, we treat it as
    // a success.
    CompleteAuthAttempt(std::move(user_context), true);
  } else {
    // For all other results, use the default base class logic.
    TokenBasedAuthRequest::NotifyAuthResult(std::move(user_context), result);
  }
}

AuthSessionIntent PaymentsAutofillAuthRequest::GetAuthSessionIntent() const {
  return AuthSessionIntent::kVerifyOnly;
}

AuthRequest::Reason PaymentsAutofillAuthRequest::GetAuthReason() const {
  return AuthRequest::Reason::kPaymentsAutofill;
}

const std::u16string PaymentsAutofillAuthRequest::GetDescription() const {
  return prompt_;
}

}  // namespace ash
