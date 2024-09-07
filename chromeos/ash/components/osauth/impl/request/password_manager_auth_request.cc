// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/request/password_manager_auth_request.h"

#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

PasswordManagerAuthRequest::PasswordManagerAuthRequest(
    TokenBasedAuthRequest::AuthCompletionCallback on_auth_complete)
    : TokenBasedAuthRequest(std::move(on_auth_complete)) {}
PasswordManagerAuthRequest::~PasswordManagerAuthRequest() = default;

AuthSessionIntent PasswordManagerAuthRequest::GetAuthSessionIntent() const {
  return AuthSessionIntent::kVerifyOnly;
}

AuthRequest::Reason PasswordManagerAuthRequest::GetAuthReason() const {
  return AuthRequest::Reason::kPasswordManager;
}

const std::u16string PasswordManagerAuthRequest::GetDescription() const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_IN_SESSION_AUTH_PASSWORD_MANAGER_PROMPT);
}

}  // namespace ash
