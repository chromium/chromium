// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_PREFS_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_PREFS_H_

class PrefRegistrySimple;

namespace safety_hub_prefs {

// Dictionary that determines the next time SafetyHub will trigger a background
// password check.
constexpr char kBackgroundPasswordCheckTimeAndInterval[] =
    "profile.background_password_check";

// Keys used inside the `kBackgroundPasswordCheckTimeAndInterval` pref dict.
constexpr char kNextPasswordCheckTimeKey[] = "next_check_time";
constexpr char kPasswordCheckIntervalKey[] = "check_interval";

}  // namespace safety_hub_prefs

void RegisterSafetyHubProfilePrefs(PrefRegistrySimple* registry);

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_PREFS_H_
