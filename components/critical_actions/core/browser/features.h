// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_FEATURES_H_
#define COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace critical_actions::features {

// Controls whether the Critical Action History service is enabled.
BASE_DECLARE_FEATURE(kCriticalActionHistory);

// Maximum number of recent navigation entries retained in the LRU cache.
extern const base::FeatureParam<int> kMaxNavigationCacheCapacity;

// Parameter controlling whether to replace "Review Gemini Activity" with
// "Go to Gemini chat" in the history menu.
extern const base::FeatureParam<bool> kEnableChatLinkouts;

}  // namespace critical_actions::features

#endif  // COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_FEATURES_H_
