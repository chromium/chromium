
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"

namespace password_manager {

PasswordStoreBackendError::PasswordStoreBackendError(
    PasswordStoreBackendErrorType error_type)
    : type(error_type) {}

}  // namespace password_manager
