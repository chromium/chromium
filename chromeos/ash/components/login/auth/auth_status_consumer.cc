// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/auth_status_consumer.h"

#include "base/notreached.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {

void AuthStatusConsumer::OnOnlinePasswordUnusable(
    std::unique_ptr<UserContext> user_context,
    bool online_password_mismatch) {
  if (online_password_mismatch) {
    OnPasswordChangeDetectedFor(user_context->GetAccountId());
  }
}

void AuthStatusConsumer::OnPasswordChangeDetectedFor(const AccountId& account) {
  NOTREACHED_IN_MIGRATION();
}

void AuthStatusConsumer::OnOldEncryptionDetected(
    std::unique_ptr<UserContext> user_context,
    bool has_incomplete_migration) {
  NOTREACHED_IN_MIGRATION();
}

void AuthStatusConsumer::OnLocalAuthenticationRequired(
    std::unique_ptr<UserContext> user_context) {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace ash
