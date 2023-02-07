// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/auth_status_consumer.h"

#include "base/notreached.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {

void AuthStatusConsumer::OnPasswordChangeDetectedLegacy(
    const UserContext& user_context) {
  NOTREACHED();
}

void AuthStatusConsumer::OnPasswordChangeDetected(
    std::unique_ptr<UserContext> user_context) {
  OnPasswordChangeDetectedFor(user_context->GetAccountId());
}

void AuthStatusConsumer::OnPasswordChangeDetectedFor(const AccountId& account) {
  NOTREACHED();
}

void AuthStatusConsumer::OnOldEncryptionDetected(
    std::unique_ptr<UserContext> user_context,
    bool has_incomplete_migration) {
  NOTREACHED();
}

}  // namespace ash
