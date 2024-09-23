// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/features.h"

#include "base/feature_list.h"

namespace page_load_metrics::features {
// Whether to send continuous events - kTouchMove, kGestureScrollUpdate,
// kGesturePinchUpdate, to page load tracker observers.
BASE_FEATURE(kSendContinuousInputEventsToObservers,
             "SendContinuousInputEventsToObservers",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace page_load_metrics::features
