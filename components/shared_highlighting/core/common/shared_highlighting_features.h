// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_FEATURES_H_
#define COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_FEATURES_H_

#include "base/metrics/field_trial_params.h"

namespace base {
struct Feature;
}

namespace shared_highlighting {

// Enables link to text to be generated in advance.
extern const base::Feature kPreemptiveLinkToTextGeneration;
// Sets the timeout length for pre-emptive link generation.
extern const base::FeatureParam<int> kPreemptiveLinkGenTimeoutLengthMs;

// Enables the new UI features for highlighted text.
extern const base::Feature kSharedHighlightingV2;

// Enables shared highlighting for AMP viewers pages.
extern const base::Feature kSharedHighlightingAmp;

// Feature flag that enable Shared Highlighting V2 in iOS.
extern const base::Feature kIOSSharedHighlightingV2;

// Returns the pre-emptive link generation timeout length.
int GetPreemptiveLinkGenTimeoutLengthMs();

}  // namespace shared_highlighting

#endif  // COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_FEATURES_H_
