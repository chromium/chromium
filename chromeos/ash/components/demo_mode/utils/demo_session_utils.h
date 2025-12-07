// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DEMO_MODE_UTILS_DEMO_SESSION_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_DEMO_MODE_UTILS_DEMO_SESSION_UTILS_H_

#include "base/component_export.h"

class PrefRegistrySimple;

namespace ash::demo_mode {

// The list of countries that Demo Mode supports, ie the countries we have
// created OUs and admin users for in the admin console.
// Sorted by country code except US is first.
inline constexpr char kSupportedCountries[][3] = {
    "US", "AT", "AU", "BE", "BR", "CA", "DE", "DK", "ES",
    "FI", "FR", "GB", "IE", "IN", "IT", "JP", "LU", "MX",
    "NL", "NO", "NZ", "PL", "PT", "SE", "ZA"};

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEMO_MODE)
// Whether the device is set up to run demo sessions.
bool IsDeviceInDemoMode();

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEMO_MODE)
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEMO_MODE)
// Force enable demo account sign in. Called before sign in and session start.
void SetForceEnableDemoAccountSignIn(bool force_enabled);

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEMO_MODE)
// True when device is in demo mode and demo account sign in is enable via
// growth, or feature flag.
// `SetForceEnableDemoAccountSignInByGrowth` will be called before the
// `AccountId` is populated, and this function should only be called after it.
// TODO(387572263): Update flag usage in
// chrome/browser/policy/profile_policy_connector.cc.
bool IsDemoAccountSignInEnabled();

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEMO_MODE)
// Set to power idle action policy to do nothing and use the DemoModeIdleHandler
// when idle.
// TODO(crbugs.com/355727308): This happens only when power policy override for
// 24h session. Rename with `Enable24HSessionForDemoMode()`.
void SetDoNothingWhenPowerIdle();

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEMO_MODE)
// Whether force the session length count from ChromeOS session instead of first
// user activity.
// TODO(crbugs.com/355727308): This happens only when power policy override for
// 24h session. Rename with `ShouldEnable24HSessionForDemoMode()`.
bool ForceSessionLengthCountFromSessionStarts();

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEMO_MODE)
// Call on setup demo account error failed by exceed QPS.Turn on "should
// schedule logout" in current session.
void TurnOnScheduleLogoutForMGS();

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEMO_MODE)
// Whether should schedule a logout in current session to retry sign-in.
bool GetShouldScheduleLogoutForMGS();

}  // namespace ash::demo_mode

#endif  // CHROMEOS_ASH_COMPONENTS_DEMO_MODE_UTILS_DEMO_SESSION_UTILS_H_
