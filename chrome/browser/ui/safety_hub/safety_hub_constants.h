// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_CONSTANTS_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_CONSTANTS_H_

#include "base/time/time.h"

namespace safety_hub {

extern const char kCardHeaderKey[];
extern const char kCardSubheaderKey[];
extern const char kCardStateKey[];

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

}  // namespace safety_hub

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_CONSTANTS_H_
