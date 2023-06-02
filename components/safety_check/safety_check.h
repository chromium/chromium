// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFETY_CHECK_SAFETY_CHECK_H_
#define COMPONENTS_SAFETY_CHECK_SAFETY_CHECK_H_

#include "base/observer_list_types.h"
#include "components/prefs/pref_service.h"

// Utilities for performing browser safety checks common to desktop, Android,
// and iOS. Platform-specific checks, such as updates and extensions, are
// implemented in handlers.
namespace safety_check {

// The following enums represent the state of each component (common among
// desktop, Android, and iOS) of the safety check and should be kept in sync
// with the JS frontend (safety_check_browser_proxy.ts) and |SafetyCheck*|
// metrics enums in enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.safety_check
enum class PasswordsStatus {
  kChecking = 0,
  kSafe = 1,
  // Indicates that at least one compromised password exists. Weak, reused or
  // muted compromised password warnings may exist as well.
  kCompromisedExist = 2,
  kOffline = 3,
  kNoPasswords = 4,
  kSignedOut = 5,
  kQuotaLimit = 6,
  kError = 7,
  kFeatureUnavailable = 8,
  // Indicates that no compromised or reused passwords exist, but there is at
  // least one weak password.
  kWeakPasswordsExist = 9,
  // Indicates that no compromised passwords exist, but there is at least one
  // reused password.
  kReusedPasswordsExist = 10,
  // Indicates no weak or reused passwords exist, but there is
  // at least one compromised password warning that has been muted by the user.
  kMutedCompromisedExist = 11,
  // New enum values must go above here.
  kMaxValue = kMutedCompromisedExist,
};

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.safety_check
enum class SafeBrowsingStatus {
  kChecking = 0,
  kEnabled = 1,
  kDisabled = 2,
  kDisabledByAdmin = 3,
  kDisabledByExtension = 4,
  kEnabledStandard = 5,
  kEnabledEnhanced = 6,
  kEnabledStandardAvailableEnhanced = 7,
  // New enum values must go above here.
  kMaxValue = kEnabledStandardAvailableEnhanced,
};

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.safety_check
enum class UpdateStatus {
  kChecking = 0,
  kUpdated = 1,
  kUpdating = 2,
  kRelaunch = 3,
  kDisabledByAdmin = 4,
  kFailedOffline = 5,
  kFailed = 6,
  // Non-Google branded browsers cannot check for updates using
  // VersionUpdater.
  kUnknown = 7,
  // Only used in Android where the user is directed to the Play Store.
  kOutdated = 8,
  // Only used in ChromeOS where a user performed a consumer rollback, and now
  // attempts to update to the previously installed (rollback) version.
  kUpdateToRollbackVersionDisallowed = 9,
  // New enum values must go above here.
  kMaxValue = kUpdateToRollbackVersionDisallowed,
};

// Gets the status of Safe Browsing from the PrefService and invokes
// OnSafeBrowsingCheckResult on each Observer with results.
SafeBrowsingStatus CheckSafeBrowsing(PrefService* pref_service);

}  // namespace safety_check

#endif  // COMPONENTS_SAFETY_CHECK_SAFETY_CHECK_H_
