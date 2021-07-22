// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_STATS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_STATS_H_

#include "components/optimization_guide/proto/models.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using optimization_guide::proto::OptimizationTarget;

namespace segmentation_platform {
namespace stats {

// Keep in sync with AdaptiveToolbarSegmentSwitch in enums.xml.
enum class AdaptiveToolbarSegmentSwitch {
  kUnknown = 0,
  kNoneToNewTab = 1,
  kNoneToShare = 2,
  kNoneToVoice = 3,
  kNewTabToNone = 4,
  kShareToNone = 5,
  kVoiceToNone = 6,
  kNewTabToShare = 7,
  kNewTabToVoice = 8,
  kShareToNewTab = 9,
  kShareToVoice = 10,
  kVoiceToNewTab = 11,
  kVoiceToShare = 12,
  kMaxValue = kVoiceToShare,
};

// Records the score computed for a given segment.
void RecordModelScore(OptimizationTarget segment_id, float score);

// Records the result of segment selection whenever segment selection is
// computed.
void RecordSegmentSelectionComputed(
    OptimizationTarget new_selection,
    absl::optional<OptimizationTarget> previous_selection);

}  // namespace stats
}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_STATS_H_
