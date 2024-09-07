// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/request/settings_auth_request.h"

#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

SettingsAuthRequest::SettingsAuthRequest(
    TokenBasedAuthRequest::AuthCompletionCallback on_auth_complete)
    : TokenBasedAuthRequest(std::move(on_auth_complete)) {}

SettingsAuthRequest::~SettingsAuthRequest() = default;

AuthSessionIntent SettingsAuthRequest::GetAuthSessionIntent() const {
  return AuthSessionIntent::kDecrypt;
}

AuthRequest::Reason SettingsAuthRequest::GetAuthReason() const {
  return AuthRequest::Reason::kSettings;
}

const std::u16string SettingsAuthRequest::GetDescription() const {
  return l10n_util::GetStringUTF16(IDS_ASH_IN_SESSION_AUTH_SETTINGS_PROMPT);
}

}  // namespace ash
