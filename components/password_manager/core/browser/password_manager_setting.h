// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_SETTING_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_SETTING_H_

namespace password_manager {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.password_manager
enum class PasswordManagerSetting {
  // Setting controlling whether the password manager offers password
  // saving.
  kOfferToSavePasswords = 0,

  // Setting controlling whether the password manager can use the
  // automatically sign in users based on their saved passwords on sites
  // which support this.
  kAutoSignIn = 1,

  // Setting controlling whether the biometric re-authentication will be
  // required before password filling.
  kBiometricReauthBeforePwdFilling = 2,

  kMaxValue = kBiometricReauthBeforePwdFilling,
};
}  // namespace password_manager

#endif  //  COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_SETTING_H_
