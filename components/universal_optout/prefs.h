// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIVERSAL_OPTOUT_PREFS_H_
#define COMPONENTS_UNIVERSAL_OPTOUT_PREFS_H_

class PrefRegistrySimple;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace universal_optout::prefs {

// Dictionary pref storing historical eligibility data per profile.
// The key is a serialized base::Time (UTC midnight) and the value is a boolean
// (true if the user is eligible, false otherwise).
inline constexpr char kUniversalOptOutEligibilityHistory[] =
    "universal_optout.eligibility_history";

// Boolean pref recording the overall eligibility state for Universal Opt Out.
inline constexpr char kUniversalOptOutEligible[] = "universal_optout.eligible";

// Boolean pref for the Universal Opt Out settings toggle.
inline constexpr char kUniversalOptOutEnabled[] = "universal_optout.enabled";

// Registers profile preferences for Universal Opt Out.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace universal_optout::prefs

#endif  // COMPONENTS_UNIVERSAL_OPTOUT_PREFS_H_
