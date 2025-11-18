// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CORE_FEATURES_H_
#define COMPONENTS_INFOBARS_CORE_FEATURES_H_

#include <optional>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace infobars {

// When enabled, infobar display is prioritized per WebContents by tier:
// 1) CRITICAL_SECURITY may stack concurrently up to a fixed cap.
// 2) DEFAULT may stack concurrently up to a fixed cap  by kMaxVisibleDefault.
// 3) LOW may stack concurrently up to a fixed cap by kMaxVisibleLow.
// 4) Preemption: DEFAULT preempts a visible LOW; nothing preempts CRITICAL.
// 5) Deterministic promotion order: CRITICAL first, then DEFAULT, then LOW.
// 6) Caps (critical/default/low/queued_low) are controlled via typed params.
BASE_DECLARE_FEATURE(kInfobarPrioritization);

// These indicate the cap for each prioritization bucket and the LOW queue:
// - kMaxVisibleCritical : max concurrently visible CRITICAL_SECURITY bars.
// - kMaxVisibleDefault  : max concurrently visible DEFAULT bars.
// - kMaxVisibleLow      : max concurrently visible LOW bars.
// - kMaxLowQueued       : max number of queued LOW entries.
BASE_DECLARE_FEATURE_PARAM(size_t, kMaxVisibleCritical);
BASE_DECLARE_FEATURE_PARAM(size_t, kMaxVisibleDefault);
BASE_DECLARE_FEATURE_PARAM(size_t, kMaxVisibleLow);
BASE_DECLARE_FEATURE_PARAM(size_t, kMaxLowQueued);

struct InfobarPriorityCaps {
  size_t max_visible_critical;
  size_t max_visible_default;
  size_t max_visible_low;
  size_t max_low_queued;
};

// Returns whether the feature is enabled.
bool IsInfobarPrioritizationEnabled();

std::optional<InfobarPriorityCaps> GetInfobarPriorityCaps();

}  // namespace infobars

#endif  // COMPONENTS_INFOBARS_CORE_FEATURES_H_
