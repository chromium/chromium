// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_REQUEST_TOKEN_BASED_AUTH_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_REQUEST_TOKEN_BASED_AUTH_REQUEST_H_

#include <memory>

#include "base/time/time.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/public/request/auth_request.h"

namespace ash {

// Handles authentication requests where a token is expected upon a successful
// authentication.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) TokenBasedAuthRequest
    : public AuthRequest {
 public:
  using AuthCompletionCallback =
      base::OnceCallback<void(bool success,
                              const ash::AuthProofToken& token,
                              base::TimeDelta timeout)>;

  explicit TokenBasedAuthRequest(AuthCompletionCallback on_auth_complete);
  ~TokenBasedAuthRequest() override;

  void NotifyAuthSuccess(std::unique_ptr<UserContext> user_context) override;
  void NotifyAuthFailure() override;

 private:
  AuthCompletionCallback on_auth_complete_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_REQUEST_TOKEN_BASED_AUTH_REQUEST_H_
