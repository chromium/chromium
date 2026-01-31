// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_REQUEST_PAYMENTS_AUTOFILL_AUTH_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_REQUEST_PAYMENTS_AUTOFILL_AUTH_REQUEST_H_

#include <memory>
#include <string>

#include "chromeos/ash/components/osauth/impl/request/token_based_auth_request.h"

namespace ash {

class UserContext;

// Passed to `ActiveSessionAuthController::ShowAuthDialog` when authenticating
// for payments autofill, handles behavior specific to those requests.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH)
    PaymentsAutofillAuthRequest : public TokenBasedAuthRequest {
 public:
  explicit PaymentsAutofillAuthRequest(
      const std::u16string& prompt,
      TokenBasedAuthRequest::AuthCompletionCallback on_auth_complete);
  PaymentsAutofillAuthRequest(const PaymentsAutofillAuthRequest&) = delete;
  PaymentsAutofillAuthRequest& operator=(const PaymentsAutofillAuthRequest&) =
      delete;
  ~PaymentsAutofillAuthRequest() override;

  // AuthRequest:
  void NotifyAuthResult(std::unique_ptr<UserContext> user_context,
                        AuthResult result) override;
  AuthSessionIntent GetAuthSessionIntent() const override;
  AuthRequest::Reason GetAuthReason() const override;
  const std::u16string GetDescription() const override;

 private:
  const std::u16string prompt_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_REQUEST_PAYMENTS_AUTOFILL_AUTH_REQUEST_H_
