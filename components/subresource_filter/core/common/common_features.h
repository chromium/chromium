// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_COMMON_FEATURES_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace subresource_filter {

// Enables the tagging of ad frames and resource requests by using the
// subresource_filter component in dry-run mode.
BASE_DECLARE_FEATURE(kAdTagging);

// Enables the artificial delaying of ads that are considered unsafe (e.g. http
// or same-domain to the top-level).
BASE_DECLARE_FEATURE(kDelayUnsafeAds);

// Enables ad tagging decisions to be propagated to network requests by removing
// an optimization which parallelizes filter list checks and navigation request
// start.
BASE_DECLARE_FEATURE(kTPCDAdHeuristicSubframeRequestTagging);

// Param which governs whether to check if a third-party cookie exception
// applies to a network request before removing the optimization which
// parallelizes its start with filter list checks.
extern const base::FeatureParam<bool> kCheckFor3pcException;

// Param which governs how much to delay non-secure (i.e. http) subresources for
// DelayUnsafeAds.
extern const char kInsecureDelayParam[];

// Param which governs how much to delay non-isolated (i.e. in a same-origin
// iframe) subresources for DelayUnsafeAds.
extern const char kNonIsolatedDelayParam[];

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_COMMON_FEATURES_H_
