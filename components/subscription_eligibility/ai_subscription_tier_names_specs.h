// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBSCRIPTION_ELIGIBILITY_AI_SUBSCRIPTION_TIER_NAMES_SPECS_H_
#define COMPONENTS_SUBSCRIPTION_ELIGIBILITY_AI_SUBSCRIPTION_TIER_NAMES_SPECS_H_

#include <string_view>

#include "base/containers/fixed_flat_map.h"

// Placeholder names of the AI subscription tiers for non-branded builds.
// Google Chrome builds use ai_subscription_tier_names_branded.h from
// components/internal/subscription_eligibility/ instead. Both headers must
// define the same constants.
//
// Consumed through //components/subscription_eligibility:tier_names; do not
// include this header directly.

namespace subscription_eligibility {

// Maps each tier to its short name.
inline constexpr auto kAiTierShortNames =
    base::MakeFixedFlatMap<int, std::u16string_view>(
        {{1, u"Tier 1"}, {2, u"Tier 2"}, {3, u"Tier 3"}});

// Maps each tier to its full name. Must list the same tiers as
// `kAiTierShortNames`.
inline constexpr auto kAiTierFullNames =
    base::MakeFixedFlatMap<int, std::u16string_view>(
        {{1, u"Google Subscription tier 1"},
         {2, u"Google Subscription tier 2"},
         {3, u"Google Subscription tier 3"}});

}  // namespace subscription_eligibility

#endif  // COMPONENTS_SUBSCRIPTION_ELIGIBILITY_AI_SUBSCRIPTION_TIER_NAMES_SPECS_H_
