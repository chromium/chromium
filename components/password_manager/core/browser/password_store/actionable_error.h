// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_ACTIONABLE_ERROR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_ACTIONABLE_ERROR_H_

namespace password_manager {

// Classifies errors that user could act on to enable their password manager.
//
// These values *may* be persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.password_manager
enum class ActionableError {
  // No known errors.
  kNoError = 0,

  // An error occurred that can't be remedied by user action.
  kInactionable = 1,

  // Like kInactionable, the error can't be remedied by user action but it's
  // likely that the error is temporary and e.g. offering to save is fine.
  kInactionableTemporaryError = 2,

  // The user needs to sign in to use the password manager.
  kSignInNeeded = 3,

  // The platform keychain needs unlocking (used on Mac & Linux).
  kKeychainError = 4,

  // The user may be signed in but their trusted vault is locked.
  kTrustedVaultKeyNeeded = 5,

  kMaxValue = kTrustedVaultKeyNeeded,
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_ACTIONABLE_ERROR_H_
