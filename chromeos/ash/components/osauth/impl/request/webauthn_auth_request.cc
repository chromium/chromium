// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/request/webauthn_auth_request.h"

#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

WebAuthNAuthRequest::WebAuthNAuthRequest(const std::string& rp_id,
                                         FinishCallback callback)
    : finish_callback_(std::move(callback)), rp_id_(rp_id) {}
WebAuthNAuthRequest::~WebAuthNAuthRequest() = default;

AuthSessionIntent WebAuthNAuthRequest::GetAuthSessionIntent() const {
  return AuthSessionIntent::kWebAuthn;
}

AuthRequest::Reason WebAuthNAuthRequest::GetAuthReason() const {
  return AuthRequest::Reason::kWebAuthN;
}

const std::u16string WebAuthNAuthRequest::GetDescription() const {
  return l10n_util::GetStringFUTF16(IDS_ASH_IN_SESSION_WEBAUTHN_PROMPT,
                                    base::UTF8ToUTF16(rp_id_));
}

void WebAuthNAuthRequest::NotifyAuthSuccess(
    std::unique_ptr<UserContext> user_context) {
  CHECK(finish_callback_);
  std::move(finish_callback_).Run(true);
}

void WebAuthNAuthRequest::NotifyAuthFailure() {
  CHECK(finish_callback_);
  std::move(finish_callback_).Run(false);
}

const std::string WebAuthNAuthRequest::GetRpId() const {
  return rp_id_;
}

}  // namespace ash
