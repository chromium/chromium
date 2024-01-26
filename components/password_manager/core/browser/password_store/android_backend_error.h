// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_ANDROID_BACKEND_ERROR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_ANDROID_BACKEND_ERROR_H_

#include <optional>

namespace password_manager {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// NOTE: This needs to be manually kept in sync with
// PasswordStoreAndroidBackendError in enums.xml!
//
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
  // Task was cleaned-up without a proper response.
  kCleanedUpWithoutResponse = 8,
  // Backend downstream implementation is not available.
  kBackendNotAvailable = 9,
  // Failed to create FacetId to obtain affiliated matches.
  kFailedToCreateFacetId = 10,
  // The job was cancelled because the targeted password storage changed.
  // This can happen if the user stops syncing passwords or changes accounts.
  kCancelledPwdSyncStateChanged = 11,
  kMaxValue = kCancelledPwdSyncStateChanged,
};

struct AndroidBackendError {
  // Type of the error returned by the bridge.
  AndroidBackendErrorType type;

  // Numeric error code returned by the GMS Core API, only available if 'type'
  // is kExternalError.
  std::optional<int> api_error_code;

  // Numeric connection result status code returned by the GMS Core API, only
  // available if ConnectionResult was set on the returned exception.
  std::optional<int> connection_result_code;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_ANDROID_BACKEND_ERROR_H_
