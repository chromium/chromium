// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_NOTIFICATION_RESULT_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_NOTIFICATION_RESULT_H_

// This represents the result of trying to show a notification to the user when
// the state of the account tailored security bit changes. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class TailoredSecurityNotificationResult {
  kUnknownResult = 0,
  kShown = 1,
  // All other results are the reason for not being shown.
  // kAccountNotConsented = 2,  // Deprecated: now using history sync optin
  kEnhancedProtectionAlreadyEnabled = 3,
  kNoWebContentsAvailable = 4,
  kSafeBrowsingControlledByPolicy = 5,
  kNoBrowserAvailable = 6,
  kNoBrowserWindowAvailable = 7,
  // kPreferencesNotSynced = 8, // Deprecated: now using history sync optin
  kHistoryNotSynced = 9,

  kMaxValue = kHistoryNotSynced,
};

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_NOTIFICATION_RESULT_H_
