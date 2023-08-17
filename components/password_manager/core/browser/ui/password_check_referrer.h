// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORD_CHECK_REFERRER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORD_CHECK_REFERRER_H_

namespace password_manager {

// Represents different referrers when navigating to the Password Check page.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Needs to stay in sync with PasswordCheckReferrer in enums.xml and
// password_manager_proxy.js.
enum class PasswordCheckReferrer {
  kSafetyCheck = 0,           // Web UI, recorded in JavaScript.
  kPasswordSettings = 1,      // Web UI, recorded in JavaScript.
  kPhishGuardDialog = 2,      // Native UI, recorded in C++.
  kPasswordBreachDialog = 3,  // Native UI, recorded in C++.
  kMoreToFixBubble = 4,       // Native UI, recorded in C++.
  // kUnsafeStateBubble = 5,  // obsolete.
  kSafetyCheckMagicStack = 6,
  kMaxValue = kSafetyCheckMagicStack,
};

// Name of the corresponding Password Check referrer histogram.
extern const char kPasswordCheckReferrerHistogram[];

// Logs the `referrer` of a navigation to the Password Check page to
// `kPasswordCheckReferrerHistogram`.
void LogPasswordCheckReferrer(PasswordCheckReferrer referrer);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORD_CHECK_REFERRER_H_
