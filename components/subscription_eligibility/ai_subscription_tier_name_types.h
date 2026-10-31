// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBSCRIPTION_ELIGIBILITY_AI_SUBSCRIPTION_TIER_NAME_TYPES_H_
#define COMPONENTS_SUBSCRIPTION_ELIGIBILITY_AI_SUBSCRIPTION_TIER_NAME_TYPES_H_

#include <string_view>

namespace subscription_eligibility {

// Short and full name of one AI subscription tier, as listed in
// `kAiTierNames`. Implementation detail of
// //components/subscription_eligibility:tier_names.
//
// To get a tier's name, use GetAiSubscriptionTierShortName() or
// GetAiSubscriptionTierFullName() from ai_subscription_tier_names.h, which
// also apply the name override.
struct AiTierName {
  std::u16string_view short_name;
  std::u16string_view full_name;
};

}  // namespace subscription_eligibility

#endif  // COMPONENTS_SUBSCRIPTION_ELIGIBILITY_AI_SUBSCRIPTION_TIER_NAME_TYPES_H_
