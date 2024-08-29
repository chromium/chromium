// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_REQUEST_WEBAUTHN_AUTH_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_REQUEST_WEBAUTHN_AUTH_REQUEST_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/osauth/public/request/auth_request.h"

namespace ash {

// Passed to `ActiveSessionAuthController::ShowAuthDialog` when authenticating
// with WebAuthN, handles behavior specific to those requests.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) WebAuthNAuthRequest
    : public AuthRequest {
 public:
  // Callback for overall authentication flow result.
  using FinishCallback = base::OnceCallback<void(bool success)>;

  WebAuthNAuthRequest(const std::string& rp_id, FinishCallback callback);
  ~WebAuthNAuthRequest() override;
  WebAuthNAuthRequest(const WebAuthNAuthRequest&) = delete;
  WebAuthNAuthRequest& operator=(const WebAuthNAuthRequest&) = delete;

  // AuthRequest:
  AuthSessionIntent GetAuthSessionIntent() const override;
  AuthRequest::Reason GetAuthReason() const override;
  const std::u16string GetDescription() const override;
  void NotifyAuthSuccess(std::unique_ptr<UserContext> user_context) override;
  void NotifyAuthFailure() override;

  const std::string GetRpId() const;

 private:
  FinishCallback finish_callback_;
  const std::string rp_id_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_REQUEST_WEBAUTHN_AUTH_REQUEST_H_
