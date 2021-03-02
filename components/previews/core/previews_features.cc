// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_features.h"

#include "build/build_config.h"

namespace previews {
namespace features {

// Kill switch (or holdback) for all previews. No previews will be allowed
// if this feature is disabled. If enabled, which specific previews that
// are enabled are controlled by other features.
const base::Feature kPreviews {
  "Previews",
#if defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS)
      // Previews allowed for Android (but also allow on Linux for dev/debug).
      base::FEATURE_ENABLED_BY_DEFAULT
#else   // !defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS)
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS)
};

// Provides slow page triggering parameters.
const base::Feature kSlowPageTriggering{"PreviewsSlowPageTriggering",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a per-page load holdback experiment using a random coin flip.
const base::Feature kCoinFlipHoldback{"PreviewsCoinFlipHoldback_UKMOnly",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables filtering navigation URLs by suffix to exclude navigation that look
// like media resources from triggering previews. For example,
// http://chromium.org/video.mp4 would be excluded.
const base::Feature kExcludedMediaSuffixes{"PreviewsExcludedMediaSuffixes",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Support for enabling DeferAllScript previews which includes a base feature
// and a UserConsistent-specific experiment feature.
const base::FeatureState kDeferAllScriptDefaultFeatureState =
#if defined(OS_ANDROID)
    base::FEATURE_DISABLED_BY_DEFAULT;
#else   // !defined(OS_ANDROID)
    base::FEATURE_DISABLED_BY_DEFAULT;
#endif  // defined(OS_ANDROID)
const base::Feature kDeferAllScriptPreviews{"DeferAllScript",
                                            kDeferAllScriptDefaultFeatureState};


}  // namespace features
}  // namespace previews
