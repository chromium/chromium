// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/link_capturing_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "content/public/common/content_features.h"

namespace apps::features {

BASE_FEATURE(kNavigationCapturingOnExistingFrames,
             "NavigationCapturingOnCurrentFrames",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool ShouldShowLinkCapturingUX() {
#if BUILDFLAG(IS_CHROMEOS)
  return true;
#else
  return base::FeatureList::IsEnabled(::features::kPwaNavigationCapturing);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

bool IsNavigationCapturingReimplEnabled() {
  return base::FeatureList::IsEnabled(::features::kPwaNavigationCapturing) &&
         (::features::kNavigationCapturingDefaultState.Get() ==
              ::features::CapturingState::kReimplDefaultOn ||
          ::features::kNavigationCapturingDefaultState.Get() ==
              ::features::CapturingState::kReimplDefaultOff ||
          ::features::kNavigationCapturingDefaultState.Get() ==
              ::features::CapturingState::kReimplOnViaClientMode);
}

}  // namespace apps::features
