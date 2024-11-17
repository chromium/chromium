// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_REQUEST_PASSWORD_MANAGER_AUTH_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_REQUEST_PASSWORD_MANAGER_AUTH_REQUEST_H_

#include <string>

#include "chromeos/ash/components/osauth/impl/request/token_based_auth_request.h"

namespace ash {

// Passed to `ActiveSessionAuthController::ShowAuthDialog` when authenticating
// from Google Password Manager, handles behavior specific to those requests.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH)
    PasswordManagerAuthRequest : public TokenBasedAuthRequest {
 public:
  explicit PasswordManagerAuthRequest(
      const std::u16string& prompt,
      TokenBasedAuthRequest::AuthCompletionCallback on_auth_complete);
  PasswordManagerAuthRequest(const PasswordManagerAuthRequest&) = delete;
  PasswordManagerAuthRequest& operator=(const PasswordManagerAuthRequest&) =
      delete;
  ~PasswordManagerAuthRequest() override;

  // AuthRequest:
  AuthSessionIntent GetAuthSessionIntent() const override;
  AuthRequest::Reason GetAuthReason() const override;
  const std::u16string GetDescription() const override;

 private:
  const std::u16string prompt_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_REQUEST_PASSWORD_MANAGER_AUTH_REQUEST_H_
