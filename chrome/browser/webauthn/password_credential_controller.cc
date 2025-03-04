// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/password_credential_controller.h"

namespace webauthn {

void PasswordCredentialController::FetchPasswords(
    const GURL& url,
    PasswordCredentialsReceivedCallback callback) {}

bool PasswordCredentialController::IsAuthRequired() {
  return false;
}

void PasswordCredentialController::SetPasswordSelectedCallback(
    AuthenticatorRequestClientDelegate::PasswordSelectedCallback callback) {}

}  // namespace webauthn
