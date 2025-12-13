// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/features.h"

namespace infobars {

BASE_FEATURE(kInfobarPrioritization, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kMaxVisibleCritical,
                   &kInfobarPrioritization,
                   "max_visible_critical",
                   2);

BASE_FEATURE_PARAM(size_t,
                   kMaxVisibleDefault,
                   &kInfobarPrioritization,
                   "max_visible_default",
                   1);

BASE_FEATURE_PARAM(size_t,
                   kMaxVisibleLow,
                   &kInfobarPrioritization,
                   "max_visible_low",
                   1);
BASE_FEATURE_PARAM(size_t,
                   kMaxLowQueued,
                   &kInfobarPrioritization,
                   "max_low_queued",
                   1);

bool IsInfobarPrioritizationEnabled() {
  return base::FeatureList::IsEnabled(kInfobarPrioritization);
}

std::optional<InfobarPriorityCaps> GetInfobarPriorityCaps() {
  if (!IsInfobarPrioritizationEnabled()) {
    return std::nullopt;
  }

  return {{.max_visible_critical = kMaxVisibleCritical.Get(),
           .max_visible_default = kMaxVisibleDefault.Get(),
           .max_visible_low = kMaxVisibleLow.Get(),
           .max_low_queued = kMaxLowQueued.Get()}};
}

}  // namespace infobars
