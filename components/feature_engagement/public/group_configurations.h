// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_GROUP_CONFIGURATIONS_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_GROUP_CONFIGURATIONS_H_

#include <optional>

#include "base/feature_list.h"

namespace feature_engagement {
struct GroupConfig;

// Returns client-side specified GroupConfig if it exists, else an empty
// optional. For this GroupConfig to be usable, the feature also needs to
// be enabled by default. As GroupConfigs can only be client-side, this
// function should return a non-empty optional for all supported Groups.
std::optional<GroupConfig> GetClientSideGroupConfig(
    const base::Feature* feature);

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_GROUP_CONFIGURATIONS_H_
