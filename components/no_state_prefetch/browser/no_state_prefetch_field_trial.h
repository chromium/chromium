// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_FIELD_TRIAL_H_
#define COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_FIELD_TRIAL_H_

#include "base/feature_list.h"

namespace prerender {

// Preconnects instead of prefetching from GWS.
extern const base::Feature kGWSPrefetchHoldback;

// Preconnects instead of prefetching from NavigationPredictor.
extern const base::Feature kNavigationPredictorPrefetchHoldback;

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_FIELD_TRIAL_H_
