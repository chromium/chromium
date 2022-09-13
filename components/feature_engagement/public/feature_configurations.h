// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_CONFIGURATIONS_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_CONFIGURATIONS_H_

#include "base/feature_list.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace feature_engagement {
struct FeatureConfig;

// Returns client-side specified FeatureConfig if it exists, else an empty
// optional. For this FeatureConfig to be usable, the feature also needs to
// be enabled by default.
absl::optional<FeatureConfig> GetClientSideFeatureConfig(
    const base::Feature* feature);

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_CONFIGURATIONS_H_
