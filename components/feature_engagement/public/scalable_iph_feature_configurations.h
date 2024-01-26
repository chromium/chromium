// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_SCALABLE_IPH_FEATURE_CONFIGURATIONS_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_SCALABLE_IPH_FEATURE_CONFIGURATIONS_H_

#include <optional>

#include "base/feature_list.h"

namespace feature_engagement {

struct FeatureConfig;

std::optional<FeatureConfig> GetScalableIphFeatureConfig(
    const base::Feature* feature);

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_SCALABLE_IPH_FEATURE_CONFIGURATIONS_H_
