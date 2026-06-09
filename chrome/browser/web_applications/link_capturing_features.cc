// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/link_capturing_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/public/common/content_features.h"

namespace apps::features {

BASE_FEATURE(kUpdateAppStringsOnSettings, base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/377760841): Remove dead code flag; never enabled.
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

}  // namespace apps::features
