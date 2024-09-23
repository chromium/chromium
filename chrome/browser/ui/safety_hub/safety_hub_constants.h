// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_CONSTANTS_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_CONSTANTS_H_

#include "base/time/time.h"

namespace safety_hub {

// Keys used to indicate the labels that should be shown in various UI items.
extern const char kCardHeaderKey[];
extern const char kCardSubheaderKey[];
extern const char kCardStateKey[];

// Key used for the dict of the Safe Browsing result.
extern const char kSafetyHubSafeBrowsingStatusKey[];

// Keys used for the dict stored in prefs for the menu notification.
extern const char kSafetyHubMenuNotificationActiveKey[];
extern const char kSafetyHubMenuNotificationAllTimeCountKey[];
extern const char kSafetyHubMenuNotificationImpressionCountKey[];
extern const char kSafetyHubMenuNotificationFirstImpressionKey[];
extern const char kSafetyHubMenuNotificationLastImpressionKey[];
extern const char kSafetyHubMenuNotificationShowAfterTimeKey[];
extern const char kSafetyHubMenuNotificationResultKey[];

// Key used for the dict of the Extensions result.
extern const char kSafetyHubTriggeringExtensionIdsKey[];

// Key of the expiration time in the |UnusedSitePermissions| object. Indicates
// the time after which the associated origin and permissions are no longer
// shown in the UI.
extern const char kExpirationKey[];
// Key of the lifetime in the |UnusedSitePermissions| object.
extern const char kLifetimeKey[];
// Key of the revoked chooser permissions in the |UnusedSitePermissions| object.
extern const char kSafetyHubChooserPermissionsData[];
// Key of the expiration time for an abusive notification permission object.
// Indicates the time after which the associated origin and permissions are no
// longer shown in the UI.
extern const char kAbusiveRevocationExpirationKey[];
// Key of the lifetime for an abusive notification permission object.
extern const char kAbusiveRevocationLifetimeKey[];

// Key of the base::Value dictionary we assign to the
// REVOKED_ABUSIVE_NOTIFICATION_PERMISSION value.
extern const char kRevokedStatusDictKeyStr[];
// When users take action to grant a permission despite warnings against doing
// so, we should ignore the origin in future auto revocations. To do this, we
// will assign the REVOKED_ABUSIVE_NOTIFICATION_PERMISSION permission
// base::Value to the "ignore" string. Otherwise, the value should be "revoke".
extern const char kIgnoreStr[];
extern const char kRevokeStr[];

// Key to store origin for a detected compromised password in
// PasswordStatusCheckResult.
extern const char kOrigin[];
// Key to store username for a detected compromised password in
// PasswordStatusCheckResult.
extern const char kUsername[];
// Key to store password data in the prefs. The data will look like:
// kSafetyHubPasswordCheckOriginsKey: [ {origin: example1.com, username: user1},
//                                      {origin: example2.com, username: user2}]
extern const char kSafetyHubPasswordCheckOriginsKey[];
#if BUILDFLAG(IS_ANDROID)
// Key to store number of compromied password in the prefs. The data will look
// like: kSafetyHubCompromiedPasswordOriginsCount: 2
extern const char kSafetyHubCompromiedPasswordOriginsCount[];
#endif  // BUILDFLAG(IS_ANDROID)

// Name of the histogram which logs how many times the blocklist is checked
// during an auto-revocation run.
extern const char kBlocklistCheckCountHistogramName[];

// State that a top card in the Safety Hub page can be in. This enum should
// remain sorted from the "worst" state (warning) to the "best" state (safe).
// Should be kept in sync with the corresponding enum in
// chrome/browser/resources/settings/safety_hub/safety_hub_browser_proxy.ts
enum class SafetyHubCardState {
  kWarning = 0,
  kWeak = 1,
  kInfo = 2,
  kSafe = 3,
  kMaxValue = kSafe,
};

// Smallest time duration between two subsequent password checks.
extern const base::TimeDelta kMinTimeBetweenPasswordChecks;

// An enum of the different Safety Hub modules that are available. This should
// be updated whenever a notification for a new module is added to or removed
// from the three-dot menu.
// Values should correspond to 'SafetyHubModuleType' in enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SafetyHubModuleType {
  UNUSED_SITE_PERMISSIONS = 0,
  NOTIFICATION_PERMISSIONS = 1,
  SAFE_BROWSING = 2,
  EXTENSIONS = 3,
  PASSWORDS = 4,
  VERSION = 5,
  kMaxValue = VERSION,
};

// Values should correspond to 'SafetyHubEntryPoint' in enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SafetyHubEntryPoint {
  kPrivacySafe = 0,
  kPrivacyWarning = 1,
  kSiteSettings = 2,
  kMenuNotifications = 3,
  kNotificationSettings = 4,
  kMaxValue = kNotificationSettings,
};

// The various surfaces that users could see (a part of) Safety Hub, or interact
// with it.
// Values should correspond to 'SafetyHubSurfaces' in enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SafetyHubSurfaces {
  kThreeDotMenu = 0,
  kSafetyHubPage = 1,
  kMaxValue = kSafetyHubPage,
};

// Duration to wait for cleaning up the revoked permissions after showing them
// to the user.
extern const base::TimeDelta kRevocationCleanUpThresholdWithDelayForTesting;

}  // namespace safety_hub

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_CONSTANTS_H_
