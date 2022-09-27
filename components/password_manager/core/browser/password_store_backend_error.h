// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_ERROR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_ERROR_H_

namespace password_manager {

enum class PasswordStoreBackendErrorType {
  kUncategorized = 0,
  // An authentication error that prevents the password store from accessing
  // passwords and can be resolved by the user. Used on Android.
  kAuthErrorResolvable = 1,
};

enum class PasswordStoreBackendErrorRecoveryType {
  // Error which isn't specified properly, should be treated as kUnrecoverable.
  kUnspecified,
  // Recoverable which can be fixed by either automated or user-driven
  // resolution specific for this error.
  kRecoverable,
  // Unrecoverable errors which can't be fixed easily. It may indicate broken
  // database or other persistent errors.
  kUnrecoverable,
  // Transitory error which requires no user input and could be resolved by
  // retrying the operation.
  kRetriable,
};

struct PasswordStoreBackendError {
  PasswordStoreBackendError(
      PasswordStoreBackendErrorType error_type,
      PasswordStoreBackendErrorRecoveryType recovery_type);

  // The type of the error.
  PasswordStoreBackendErrorType type;

  // Whether the error is considered recoverable or not.
  PasswordStoreBackendErrorRecoveryType recovery_type;
};

bool operator==(const PasswordStoreBackendError& lhs,
                const PasswordStoreBackendError& rhs);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_ERROR_H_
