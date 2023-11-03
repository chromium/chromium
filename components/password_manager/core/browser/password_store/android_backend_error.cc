// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/android_backend_error.h"

namespace password_manager {

AndroidBackendError::AndroidBackendError(AndroidBackendErrorType error_type)
    : type(error_type) {}

AndroidBackendError::AndroidBackendError(AndroidBackendError&& error) = default;

}  // namespace password_manager
