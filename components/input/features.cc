// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/features.h"

namespace input::features {

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kInputOnViz, "InputOnViz", base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kLogBubblingTouchscreenGesturesForDebug,
             "LogBubblingTouchscreenGesturesForDebug",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// Flag guard for fix for crbug.com/346629231.
BASE_FEATURE(kIgnoreBubblingCollisionIfSourceDevicesMismatch,
             "IgnoreBubblingCollisionIfSourceDevicesMismatch",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Flag guard for fix for crbug.com/346629231.
BASE_FEATURE(kScrollBubblingFix,
             "ScrollBubblingFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace input::features
