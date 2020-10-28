// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header contains field trial and variations definitions for policies,
// mechanisms and features in the performance_manager component.

#include "base/feature_list.h"
#include "base/time/time.h"

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FEATURES_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FEATURES_H_

namespace performance_manager {
namespace features {

// The feature that gates the TabLoadingFrameNavigationPolicy, and its
// mechanism counterpart the TabLoadingFrameNavigationScheduler.
extern const base::Feature kTabLoadingFrameNavigationThrottles;

// Parameters controlling the TabLoadingFrameNavigationThrottles feature.
struct TabLoadingFrameNavigationThrottlesParams {
  TabLoadingFrameNavigationThrottlesParams();
  ~TabLoadingFrameNavigationThrottlesParams();

  static TabLoadingFrameNavigationThrottlesParams GetParams();

  // The minimum and maximum amount of time throttles will be applied to
  // non-primary content frames.
  base::TimeDelta minimum_throttle_timeout;
  base::TimeDelta maximum_throttle_timeout;

  // The multiple of elapsed time from navigation start until
  // FirstContentfulPaint (FCP) that is used in calculating the timeout to apply
  // to the throttles.
  double fcp_multiple;
};

// A feature that gates the inclusion of service worker relationships in the
// graph. This is a temporary feature, see https://crbug.com/1143281.
extern const base::Feature kServiceWorkerRelationshipsInGraph;

}  // namespace features
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FEATURES_H_
