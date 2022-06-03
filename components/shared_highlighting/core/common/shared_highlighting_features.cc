// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/shared_highlighting_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace shared_highlighting {

const base::Feature kPreemptiveLinkToTextGeneration{
    "PreemptiveLinkToTextGeneration", base::FEATURE_ENABLED_BY_DEFAULT};
constexpr base::FeatureParam<int> kPreemptiveLinkGenTimeoutLengthMs{
    &kPreemptiveLinkToTextGeneration, "TimeoutLengthMs", 500};

const base::Feature kSharedHighlightingAmp {
  "SharedHighlightingAmp",
#if BUILDFLAG(IS_IOS)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

const base::Feature kIOSSharedHighlightingV2{"IOSSharedHighlightingV2",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSharedHighlightingRefinedBlocklist{
    "SharedHighlightingRefinedBlocklist", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSharedHighlightingRefinedMaxContextWords{
    "SharedHighlightingRefinedMaxContextWords",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSharedHighlightingManager{
    "SharedHighlightingManager", base::FEATURE_DISABLED_BY_DEFAULT};

const char kSharedHighlightingRefinedMaxContextWordsName[] =
    "SharedHighlightingRefinedMaxContextWords";

const base::FeatureParam<int> kSharedHighlightingMaxContextWords{
    &kSharedHighlightingRefinedMaxContextWords,
    kSharedHighlightingRefinedMaxContextWordsName, 10};

int GetPreemptiveLinkGenTimeoutLengthMs() {
  return kPreemptiveLinkGenTimeoutLengthMs.Get();
}

}  // namespace shared_highlighting
