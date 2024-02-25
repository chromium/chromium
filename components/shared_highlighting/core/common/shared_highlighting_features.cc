// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/shared_highlighting_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace shared_highlighting {

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kSharedHighlightingAmp,
             "SharedHighlightingAmp",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kIOSSharedHighlightingV2,
             "IOSSharedHighlightingV2",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSharedHighlightingManager,
             "SharedHighlightingManager",
             base::FEATURE_DISABLED_BY_DEFAULT);

int GetPreemptiveLinkGenTimeoutLengthMs() {
#if BUILDFLAG(IS_ANDROID)
  return 100;
#else
  return 500;
#endif
}

}  // namespace shared_highlighting
