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

// State that a top card in the Safety Hub page can be in.
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
// When the password check didn't run at its scheduled time (e.g. client was
// offline) it will be scheduled to run within this time frame.
extern const base::TimeDelta kPasswordCheckOverdueTimeWindow;

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

}  // namespace safety_hub

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_CONSTANTS_H_
