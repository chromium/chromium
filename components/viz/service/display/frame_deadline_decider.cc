// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/frame_deadline_decider.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"

namespace viz {

FrameDeadlineDecider::FrameDeadlineDecider() = default;

FrameDeadlineDecider::~FrameDeadlineDecider() = default;

PossibleDeadline FrameDeadlineDecider::SelectDeadline(
    const PossibleDeadlines& possible_deadlines) {
  bool use_platform_preferred_deadlines = false;
#if BUILDFLAG(IS_ANDROID)
  use_platform_preferred_deadlines =
      !base::FeatureList::IsEnabled(features::kUseAndroidCustomFrameDeadlines);
#endif  // BUILDFLAG(IS_ANDROID)
  if (use_platform_preferred_deadlines) {
    return possible_deadlines.GetPreferredDeadline();
  }
  // TODO(crbug.com/500826814): Complete the implementation to dynamically
  // select deadlines based on input timestamps.
  return possible_deadlines.GetPreferredDeadline();
}

}  // namespace viz
