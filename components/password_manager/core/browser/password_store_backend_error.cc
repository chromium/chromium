
// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/password_manager/core/browser/password_store_backend_error.h"

namespace password_manager {

PasswordStoreBackendError::PasswordStoreBackendError(
    PasswordStoreBackendErrorType error_type,
    PasswordStoreBackendErrorRecoveryType recovery_type)
    : type(error_type), recovery_type(recovery_type) {}

bool operator==(const PasswordStoreBackendError& lhs,
                const PasswordStoreBackendError& rhs) {
  return lhs.type == rhs.type && lhs.recovery_type == rhs.recovery_type;
}

}  // namespace password_manager
