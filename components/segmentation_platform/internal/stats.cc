// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/stats.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace segmentation_platform {
namespace stats {
namespace {
// Converts OptimizationTarget to histogram suffix.
// Should maps to suffix string in histograms.xml.
std::string OptimizationTargetToHistogramSuffix(OptimizationTarget segment_id) {
  switch (segment_id) {
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
      return "NewTab";
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
      return "Share";
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
      return "Voice";
    default:
      NOTREACHED();
      return "Unknown";
  }
}

// Keep in sync with AdaptiveToolbarButtonVariant in enums.xml.
enum class AdaptiveToolbarButtonVariant {
  kUnknown = 0,
  kNone = 1,
  kNewTab = 2,
  kShare = 3,
  kVoice = 4,
  kMaxValue = kVoice,
};

AdaptiveToolbarButtonVariant OptimizationTargetToAdaptiveToolbarButtonVariant(
    OptimizationTarget segment_id) {
  switch (segment_id) {
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
      return AdaptiveToolbarButtonVariant::kNewTab;
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
      return AdaptiveToolbarButtonVariant::kShare;
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
      return AdaptiveToolbarButtonVariant::kVoice;
    case OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN:
      return AdaptiveToolbarButtonVariant::kNone;
    default:
      return AdaptiveToolbarButtonVariant::kUnknown;
  }
}

AdaptiveToolbarSegmentSwitch GetSegmentSwitch(
    OptimizationTarget new_selection,
    OptimizationTarget previous_selection) {
  switch (previous_selection) {
    case OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN:
      switch (new_selection) {
        case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
          return AdaptiveToolbarSegmentSwitch::kNoneToNewTab;
        case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
          return AdaptiveToolbarSegmentSwitch::kNoneToShare;
        case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
          return AdaptiveToolbarSegmentSwitch::kNoneToVoice;
        default:
          NOTREACHED();
          return AdaptiveToolbarSegmentSwitch::kUnknown;
      }
      break;

    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
      switch (new_selection) {
        case OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN:
          return AdaptiveToolbarSegmentSwitch::kNewTabToNone;
        case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
          return AdaptiveToolbarSegmentSwitch::kNewTabToShare;
        case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
          return AdaptiveToolbarSegmentSwitch::kNewTabToVoice;
        default:
          NOTREACHED();
          return AdaptiveToolbarSegmentSwitch::kUnknown;
      }
      break;

    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
      switch (new_selection) {
        case OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN:
          return AdaptiveToolbarSegmentSwitch::kShareToNone;
        case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
          return AdaptiveToolbarSegmentSwitch::kShareToNewTab;
        case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
          return AdaptiveToolbarSegmentSwitch::kShareToVoice;
        default:
          NOTREACHED();
          return AdaptiveToolbarSegmentSwitch::kUnknown;
      }
      break;

    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
      switch (new_selection) {
        case OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN:
          return AdaptiveToolbarSegmentSwitch::kVoiceToNone;
        case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
          return AdaptiveToolbarSegmentSwitch::kVoiceToNewTab;
        case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
          return AdaptiveToolbarSegmentSwitch::kVoiceToShare;
        default:
          NOTREACHED();
          return AdaptiveToolbarSegmentSwitch::kUnknown;
      }
      break;

    default:
      NOTREACHED();
      return AdaptiveToolbarSegmentSwitch::kUnknown;
  }
}

}  // namespace

void RecordModelScore(OptimizationTarget segment_id, float score) {
  base::UmaHistogramPercentage(
      "SegmentationPlatform.AdaptiveToolbar.ModelScore." +
          OptimizationTargetToHistogramSuffix(segment_id),
      score * 100);
}

void RecordSegmentSelectionComputed(
    OptimizationTarget new_selection,
    absl::optional<OptimizationTarget> previous_selection) {
  base::UmaHistogramEnumeration(
      "SegmentationPlatform.AdaptiveToolbar.SegmentSelection.Computed",
      OptimizationTargetToAdaptiveToolbarButtonVariant(new_selection));

  OptimizationTarget prev_segment =
      previous_selection.has_value()
          ? previous_selection.value()
          : OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN;

  if (prev_segment == new_selection)
    return;

  base::UmaHistogramEnumeration(
      "SegmentationPlatform.AdaptiveToolbar.SegmentSwitched",
      GetSegmentSwitch(new_selection, prev_segment));
}

}  // namespace stats
}  // namespace segmentation_platform
