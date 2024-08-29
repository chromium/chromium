// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_REQUEST_SETTINGS_AUTH_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_REQUEST_SETTINGS_AUTH_REQUEST_H_

#include "chromeos/ash/components/osauth/impl/request/token_based_auth_request.h"

namespace ash {

// Passed to `ActiveSessionAuthController::ShowAuthDialog` when authenticating
// from ChromeOS settings, handles behavior specific to those requests.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) SettingsAuthRequest
    : public TokenBasedAuthRequest {
 public:
  explicit SettingsAuthRequest(
      TokenBasedAuthRequest::AuthCompletionCallback on_auth_complete);
  SettingsAuthRequest(const SettingsAuthRequest&) = delete;
  SettingsAuthRequest& operator=(const SettingsAuthRequest&) = delete;
  ~SettingsAuthRequest() override;

  // AuthRequest:
  AuthSessionIntent GetAuthSessionIntent() const override;
  AuthRequest::Reason GetAuthReason() const override;
  const std::u16string GetDescription() const override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_REQUEST_SETTINGS_AUTH_REQUEST_H_
