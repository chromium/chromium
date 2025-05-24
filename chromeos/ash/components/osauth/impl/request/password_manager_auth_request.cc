// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/request/password_manager_auth_request.h"

#include <string>

#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

PasswordManagerAuthRequest::PasswordManagerAuthRequest(
    const std::u16string& prompt,
    TokenBasedAuthRequest::AuthCompletionCallback on_auth_complete)
    : TokenBasedAuthRequest(std::move(on_auth_complete)), prompt_(prompt) {}

PasswordManagerAuthRequest::~PasswordManagerAuthRequest() = default;

AuthSessionIntent PasswordManagerAuthRequest::GetAuthSessionIntent() const {
  return AuthSessionIntent::kVerifyOnly;
}

AuthRequest::Reason PasswordManagerAuthRequest::GetAuthReason() const {
  return AuthRequest::Reason::kPasswordManager;
}

const std::u16string PasswordManagerAuthRequest::GetDescription() const {
  if (prompt_.empty()) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_IN_SESSION_AUTH_PASSWORD_MANAGER_PROMPT);
  } else {
    return prompt_;
  }
}

}  // namespace ash
