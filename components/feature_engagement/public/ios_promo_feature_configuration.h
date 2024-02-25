// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_IOS_PROMO_FEATURE_CONFIGURATION_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_IOS_PROMO_FEATURE_CONFIGURATION_H_

#include <optional>

#include "base/feature_list.h"

namespace feature_engagement {
struct FeatureConfig;

// Returns client-side specified FeatureConfig for the given iOS Promo feature
// if it exists, else an empty optional. For this FeatureConfig to be usable,
// the feature also needs to be enabled by default. iOS Promo Configs are
// separated out for better organization.
std::optional<FeatureConfig> GetClientSideiOSPromoFeatureConfig(
    const base::Feature* feature);

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_IOS_PROMO_FEATURE_CONFIGURATION_H_
