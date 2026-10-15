// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_FEATURES_H_
#define COMPONENTS_LIVE_CAPTION_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace live_caption {

// Controls whether on-device translation is used for the Live Caption feature.
BASE_DECLARE_FEATURE(kLiveCaptionOnDeviceTranslation);

// Controls whether on-device translation is restricted to/from English and
// not enabled for zh-TW.
BASE_DECLARE_FEATURE(kLiveCaptionOnDeviceTranslationEnglishOnly);

// Controls time and punctuation gating (debouncing) for Live Translate to
// reduce translation frequency of partial transcriptions.
BASE_DECLARE_FEATURE(kLiveTranslateTimeGating);

inline constexpr base::FeatureParam<base::TimeDelta>
    kLiveTranslateDebounceDuration{&kLiveTranslateTimeGating,
                                   "debounce_duration",
                                   base::Milliseconds(500)};

inline constexpr base::FeatureParam<base::TimeDelta> kLiveTranslateMaxDuration{
    &kLiveTranslateTimeGating, "max_duration", base::Milliseconds(800)};

}  // namespace live_caption

#endif  // COMPONENTS_LIVE_CAPTION_FEATURES_H_
