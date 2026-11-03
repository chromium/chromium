// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBSCRIPTION_ELIGIBILITY_AI_SUBSCRIPTION_TIER_NAMES_H_
#define COMPONENTS_SUBSCRIPTION_ELIGIBILITY_AI_SUBSCRIPTION_TIER_NAMES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

// Display names of the AI subscription tiers, shared across platforms.
//
// `tier` is the value returned by
// `SubscriptionEligibilityService::GetAiSubscriptionTier()`.
//
// The default names (`kAiTierNames`) only cover the tiers known when the
// binary was built. `kAiSubscriptionTierOverrides` adjusts them server-side.
namespace subscription_eligibility {

// When enabled, `kAiTierOverrides` applies. Ignored while disabled.
BASE_DECLARE_FEATURE(kAiSubscriptionTierOverrides);

// JSON dictionary overriding the default tier presentation:
//   {
//     "tier_names": {
//       "2": {"short_name": "Super", "full_name": "AI Super"},
//       "4": {"short_name": "Max", "full_name": "AI Max"}
//     },
//     "excluded_tiers": [3]
//   }
//
// - "tier_names" (optional) maps a tier to both of its names.
//   It can rename a tier listed in `kAiTierNames` or name a new tier unknown
//   to the binary.
//   Both "short_name" and  "full_name" (accessibility) are required.
//   An empty string hides the badge text and uses the generic accessibility
//   string.
// - "excluded_tiers" (optional) lists tiers that must not be presented as AI
//   subscription tiers at all: no avatar decoration and no names.
//   Excluded tiers has higher priority over "tier_names". If a tier appears in
//   both, it will be excluded.
//
// Tiers are integers, written as strings in "tier_names" keys and as JSON
// integers in "excluded_tiers". Tiers <= 0 mean no subscription: entries for
// them are valid but have no effect.
//
// The whole value is ignored (and the default values are used)
// if the feature param is not valid: malformed JSON, wrong types,
// a non-canonical tier key (e.g. "02"), or a missing name.
extern const base::FeatureParam<std::string> kAiTierOverrides;

// How an AI subscription tier is presented, resolved from `kAiTierNames` and
// `kAiTierOverrides`.
struct AiSubscriptionTierDisplayInfo {
  // Whether the tier is presented as an AI subscription tier at all (e.g. with
  // an avatar decoration). When false, both labels are empty.
  bool show_decoration = false;
  // Short name shown in the avatar badge. Empty means no badge text.
  std::u16string badge_label;
  // Full name, used for accessibility. If empty while `show_decoration` is
  // true, consumers should fall back to using a generic AI membership string.
  std::u16string full_membership_label;

  friend bool operator==(const AiSubscriptionTierDisplayInfo&,
                         const AiSubscriptionTierDisplayInfo&) = default;
};

// Resolves the presentation of `tier`, applying in priority order:
// 1. `tier` <= 0 (no subscription): not shown.
// 2. `tier` in "excluded_tiers": not shown, even if it is in "tier_names".
// 3. `tier` in "tier_names": shown with those names, which may be empty.
// 4. Otherwise: shown with the names in `kAiTierNames`. Both labels are empty
//    for a tier unknown to the binary.
// Steps 2 and 3 only apply while `kAiSubscriptionTierOverrides` is enabled and
// `kAiTierOverrides` is valid.
//
// `kAiTierOverrides` is read and parsed once, on the first call: later changes
// to the feature state (e.g. by `base::test::ScopedFeatureList` in a test) are
// ignored until `ResetAiTierOverridesForTesting()` is called.
AiSubscriptionTierDisplayInfo GetAiSubscriptionTierDisplayInfo(int tier);

// Re-reads `kAiTierOverrides`. Tests must call it after changing the state of
// `kAiSubscriptionTierOverrides`, and once the change is undone.
void ResetAiTierOverridesForTesting();

}  // namespace subscription_eligibility

#endif  // COMPONENTS_SUBSCRIPTION_ELIGIBILITY_AI_SUBSCRIPTION_TIER_NAMES_H_
