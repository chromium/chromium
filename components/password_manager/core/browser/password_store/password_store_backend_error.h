// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_BACKEND_ERROR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_BACKEND_ERROR_H_

#include "build/build_config.h"
#include "build/buildflag.h"

#if BUILDFLAG(IS_ANDROID)
#include <optional>
#endif

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
  // Error related only to on-device encryption users when the encryption
  // key is missing. Used on Android.
  kKeyRetrievalRequired = 4,
  // Saving new credentials is disabled due to an outdated GMSCore version.
  kGMSCoreOutdatedSavingDisabled = 5,
  // Credentials are saved only on device due to an outdated GMSCore version.
  kGMSCoreOutdatedSavingPossible = 6,
  kMaxValue = kGMSCoreOutdatedSavingPossible,
};

struct PasswordStoreBackendError {
  PasswordStoreBackendError(PasswordStoreBackendErrorType error_type);

  friend bool operator==(const PasswordStoreBackendError&,
                         const PasswordStoreBackendError&) = default;

  // The type of the error.
  PasswordStoreBackendErrorType type;

#if BUILDFLAG(IS_ANDROID)
  // Android API Error.
  // TODO(crbug.com/342993480) Remove this once UPM migration errors are no
  // longer needed.
  std::optional<int> android_backend_api_error;
#endif
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_BACKEND_ERROR_H_
