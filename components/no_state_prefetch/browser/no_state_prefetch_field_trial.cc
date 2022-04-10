// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/browser/no_state_prefetch_field_trial.h"

#include <string>

namespace prerender {

const base::Feature kGWSPrefetchHoldback{"GWSPrefetchHoldback",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNavigationPredictorPrefetchHoldback{
    "NavigationPredictorPrefetchHoldback", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace prerender
