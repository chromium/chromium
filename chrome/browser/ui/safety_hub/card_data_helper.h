// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_CARD_DATA_HELPER_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_CARD_DATA_HELPER_H_

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"

namespace safety_hub {

// Fetches data for the version card to return data to the UI.
base::Value::Dict GetVersionCardData();

// Fetches data for the password card to return data to the UI.
base::Value::Dict GetPasswordCardData(Profile* profile);

// Fetches data for the Safe Browsing card to return data to the UI.
base::Value::Dict GetSafeBrowsingCardData(Profile* profile);

// Gets the overall state for the different Safety Hub modules. This will be the
// "worst" state that any module is in. For instance, a single "warning" and
// two "safe" states will result in "warning". For modules with a card, the
// value will reflect that of a card, for the other modules, the state will be
// in a "warning" state if any item needs to be reviewed.
SafetyHubCardState GetOverallState(Profile* profile);

}  // namespace safety_hub

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_CARD_DATA_HELPER_H_
