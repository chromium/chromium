// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_BACKEND_ERROR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_BACKEND_ERROR_H_

namespace password_manager {

// List of constants describing the types of Android backend errors.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This should be kept in sync with PasswordStoreBackendErrorType in enums.xml.
enum class PasswordStoreBackendErrorType {
  kUncategorized = 0,
  // An authentication error that prevents the password store from accessing
  // passwords, for which the resolution intent has been received. Used on
  // Android.
  kAuthErrorResolvable = 1,
  // An authentication error that prevents the password store from accessing
  // passwords, for which no resolution intent has been received. Used on
  // Android.
  kAuthErrorUnresolvable = 2,
  // A Keychain error that prevents the password store from decrypting the
  // passwords. Used on Mac.
  kKeychainError = 3,
  kMaxValue = kKeychainError,
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

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_BACKEND_ERROR_H_
