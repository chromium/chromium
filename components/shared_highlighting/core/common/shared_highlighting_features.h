// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_FEATURES_H_
#define COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace shared_highlighting {

// Enables link to text to be generated in advance.
BASE_DECLARE_FEATURE(kPreemptiveLinkToTextGeneration);
// Sets the timeout length for pre-emptive link generation.
extern const base::FeatureParam<int> kPreemptiveLinkGenTimeoutLengthMs;

#if BUILDFLAG(IS_IOS)
// Enables shared highlighting for AMP viewers pages.
BASE_DECLARE_FEATURE(kSharedHighlightingAmp);
#endif

// Enables the new SharedHighlightingManager refactoring.
BASE_DECLARE_FEATURE(kSharedHighlightingManager);

// Feature flag that enable Shared Highlighting V2 in iOS.
BASE_DECLARE_FEATURE(kIOSSharedHighlightingV2);

// Returns the pre-emptive link generation timeout length.
int GetPreemptiveLinkGenTimeoutLengthMs();

}  // namespace shared_highlighting

#endif  // COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_FEATURES_H_
