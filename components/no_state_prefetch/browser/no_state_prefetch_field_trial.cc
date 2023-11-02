// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/browser/no_state_prefetch_field_trial.h"

#include <string>

namespace prerender {

BASE_FEATURE(kGWSPrefetchHoldback,
             "GWSPrefetchHoldback",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNavigationPredictorPrefetchHoldback,
             "NavigationPredictorPrefetchHoldback",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace prerender
