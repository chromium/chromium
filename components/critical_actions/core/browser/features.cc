// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/features.h"

#include "base/feature_list.h"

namespace critical_actions::features {

BASE_FEATURE(kCriticalActionHistory, base::FEATURE_DISABLED_BY_DEFAULT);

// Default capacity of 200 provides room for active tab switching and
// concurrent subframe navigations while keeping memory footprint low.
// TODO(b/535078652): Register a histogram to see the usage of LRU cache to set
// up an optimum value for max capacity.
const base::FeatureParam<int> kMaxNavigationCacheCapacity{
    &kCriticalActionHistory, "max_navigation_cache_capacity", 200};

const base::FeatureParam<bool> kEnableChatLinkouts{
    &kCriticalActionHistory, "enable_chat_linkouts", true};

}  // namespace critical_actions::features
