// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_BACKEND_ERROR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_BACKEND_ERROR_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.password_manager
enum class AndroidBackendErrorType {
  kUncategorized = 0,
  // API was called without a context.
  kNoContext = 1,
  // API was called without an account.
  kNoAccount = 2,
  // Browser profile isn't initialized yet.
  kProfileNotInitialized = 3,
  // Sync service isn't available yet.
  kSyncServiceUnavailable = 4,
  // API was called with passphrase.
  kPassphraseNotSupported = 5,
  // GMS Core version is not supported.
  kGMSVersionNotSupported = 6,
  // API was successfully called, but returned an error.
  kExternalError = 7,
  kMaxValue = kExternalError,
};

struct AndroidBackendError {
  explicit AndroidBackendError(AndroidBackendErrorType error_type);

  AndroidBackendError(const AndroidBackendError&) = delete;
  AndroidBackendError(AndroidBackendError&&);
  AndroidBackendError& operator=(const AndroidBackendError&) = delete;
  AndroidBackendError& operator=(AndroidBackendError&&) = delete;

  // Type of the error returned by the bridge.
  AndroidBackendErrorType type;

  // Numeric error code returned by the GMS Core API, only available if 'type'
  // is kExternalError.
  absl::optional<int> api_error_code;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_BACKEND_ERROR_H_
