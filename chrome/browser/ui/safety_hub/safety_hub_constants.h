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

// State that a top card in the SafetyHub page can be in.
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
enum class SafetyHubModuleType {
  UNUSED_SITE_PERMISSIONS,
  NOTIFICATION_PERMISSIONS,
  SAFE_BROWSING,
};

}  // namespace safety_hub

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_CONSTANTS_H_
