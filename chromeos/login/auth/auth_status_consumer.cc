// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/auth_status_consumer.h"
#include "base/notreached.h"

namespace chromeos {

void AuthStatusConsumer::OnPasswordChangeDetected(
    const UserContext& user_context) {
  NOTREACHED();
}

void AuthStatusConsumer::OnOldEncryptionDetected(
    const UserContext& user_context,
    bool has_incomplete_migration) {
  NOTREACHED();
}

}  // namespace chromeos
