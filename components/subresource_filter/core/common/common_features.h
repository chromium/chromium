// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_COMMON_FEATURES_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_COMMON_FEATURES_H_

#include "base/feature_list.h"

namespace subresource_filter {

// Enables the tagging of ad frames and resource requests by using the
// subresource_filter component in dry-run mode.
BASE_DECLARE_FEATURE(kAdTagging);

// Enables the artificial delaying of ads that are considered unsafe (e.g. http
// or same-domain to the top-level).
BASE_DECLARE_FEATURE(kDelayUnsafeAds);

// Param which governs how much to delay non-secure (i.e. http) subresources for
// DelayUnsafeAds.
extern const char kInsecureDelayParam[];

// Param which governs how much to delay non-isolated (i.e. in a same-origin
// iframe) subresources for DelayUnsafeAds.
extern const char kNonIsolatedDelayParam[];

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_COMMON_FEATURES_H_
