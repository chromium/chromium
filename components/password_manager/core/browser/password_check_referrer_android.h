// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHECK_REFERRER_ANDROID_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHECK_REFERRER_ANDROID_H_

namespace password_manager {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. To be kept in sync with
// PasswordCheckReferrerAndroid in enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.password_manager
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: PasswordCheckReferrer
enum class PasswordCheckReferrerAndroid {
  // Corresponds to the Settings > Passwords page.
  kPasswordSettings = 0,
  // Corresponds to the safety check settings page.
  kSafetyCheck = 1,
  // Represents the leak dialog prompted to the user when they sign in with a
  // credential which was part of a data breach.
  kLeakDialog = 2,
  // Represents the PhishGuard password reuse warning dialog prompted to the
  // user when they enter a saved password into a 'phishing' or 'low reputation'
  // site.
  kPhishedWarningDialog = 3,

  kCount = 4,
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHECK_REFERRER_ANDROID_H_
