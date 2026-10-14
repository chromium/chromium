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
namespace subscription_eligibility {

// When enabled, `kAiSubscriptionTierNameOverrideMapping` replaces the built-in
// short names.
BASE_DECLARE_FEATURE(kEnabledAiSubscriptionTierNameOverride);

// JSON dictionary from tier number to short name, e.g. `{"2": "Advanced"}`.
// Tiers missing from the dictionary use the built-in name. An empty string
// hides the name for that tier.
extern const base::FeatureParam<std::string>
    kAiSubscriptionTierNameOverrideMapping;

// Returns the short name of `tier`, shown in the avatar badge.
// Applies `kAiSubscriptionTierNameOverrideMapping` when enabled. Returns an
// empty string if the tier has no name.
std::u16string GetAiSubscriptionTierShortName(int tier);

// Returns the full name of `tier`, or an empty string if the tier is unknown.
// Also returns an empty string if `kAiSubscriptionTierNameOverrideMapping`
// overrides the short name of `tier`, to avoid showing an outdated built-in
// full name: callers should then use a generic string instead.
// TODO(crbug.com/572810504): Support A11y tier name overriding.
std::u16string GetAiSubscriptionTierFullName(int tier);

}  // namespace subscription_eligibility

#endif  // COMPONENTS_SUBSCRIPTION_ELIGIBILITY_AI_SUBSCRIPTION_TIER_NAMES_H_
