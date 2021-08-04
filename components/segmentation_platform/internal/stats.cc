// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/stats.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/proto/types.pb.h"

namespace segmentation_platform {
namespace stats {
namespace {
// Should map to SegmentationModel variant in
// //tools/metrics/histograms/metadata/segmentation_platform/histograms.xml.
std::string OptimizationTargetToHistogramVariant(
    OptimizationTarget segment_id) {
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

// This is the segmentation subset of
// optimization_guide::proto::OptimizationTarget.
// Keep in sync with SegmentationPlatformSegmenationModel in
// //tools/metrics/histograms/enums.xml.
// See also SegmentationModel variant in
// //tools/metrics/histograms/metadata/segmentation_platform/histograms.xml.
enum class SegmentationModel {
  kUnknown = 0,
  kNewTab = 4,
  kShare = 5,
  kVoice = 6,
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

    default:
      NOTREACHED();
      return AdaptiveToolbarSegmentSwitch::kUnknown;
  }
}

SegmentationModel OptimizationTargetToSegmentationModel(
    OptimizationTarget segment_id) {
  switch (segment_id) {
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
      return SegmentationModel::kNewTab;
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
      return SegmentationModel::kShare;
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
      return SegmentationModel::kVoice;
    default:
      return SegmentationModel::kUnknown;
  }
}

// Should map to ModelExecutionStatus variant string in
// //tools/metrics/histograms/metadata/segmentation_platform/histograms.xml.
std::string ModelExecutionStatusToHistogramVariant(
    ModelExecutionStatus status) {
  switch (status) {
    case ModelExecutionStatus::kSuccess:
      return "Success";
    case ModelExecutionStatus::kExecutionError:
      return "ExecutionError";
    case ModelExecutionStatus::kInvalidMetadata:
      return "InvalidMetadata";
    default:
      NOTREACHED();
      return "Unknown";
  }
}

// Should map to SignalType variant string in
// //tools/metrics/histograms/metadata/segmentation_platform/histograms.xml.
std::string SignalTypeToHistogramVariant(proto::SignalType signal_type) {
  switch (signal_type) {
    case proto::SignalType::USER_ACTION:
      return "UserAction";
    case proto::SignalType::HISTOGRAM_ENUM:
      return "HistogramEnum";
    case proto::SignalType::HISTOGRAM_VALUE:
      return "HistogramValue";
    default:
      NOTREACHED();
      return "Unknown";
  }
}

float ZeroValueFraction(const std::vector<float>& tensor) {
  if (tensor.size() == 0)
    return 0;

  size_t zero_values = 0;
  for (float feature : tensor) {
    if (feature == 0)
      ++zero_values;
  }
  return static_cast<float>(zero_values) / static_cast<float>(tensor.size());
}

}  // namespace

void RecordModelScore(OptimizationTarget segment_id, float score) {
  base::UmaHistogramPercentage(
      "SegmentationPlatform.AdaptiveToolbar.ModelScore." +
          OptimizationTargetToHistogramVariant(segment_id),
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

void RecordMaintenanceCleanupSignalSuccessCount(size_t count) {
  UMA_HISTOGRAM_COUNTS_1000(
      "SegmentationPlatform.Maintenance.CleanupSignalSuccessCount", count);
}

void RecordMaintenanceCompactionResult(proto::SignalType signal_type,
                                       bool success) {
  base::UmaHistogramBoolean(
      "SegmentationPlatform.Maintenance.CompactionResult." +
          SignalTypeToHistogramVariant(signal_type),
      success);
}

void RecordMaintenanceSignalIdentifierCount(size_t count) {
  UMA_HISTOGRAM_COUNTS_1000(
      "SegmentationPlatform.Maintenance.SignalIdentifierCount", count);
}

void RecordModelDeliveryHasMetadata(OptimizationTarget segment_id,
                                    bool has_metadata) {
  base::UmaHistogramBoolean(
      "SegmentationPlatform.ModelDelivery.HasMetadata." +
          OptimizationTargetToHistogramVariant(segment_id),
      has_metadata);
}

void RecordModelDeliveryMetadataFeatureCount(OptimizationTarget segment_id,
                                             size_t count) {
  base::UmaHistogramCounts1000(
      "SegmentationPlatform.ModelDelivery.Metadata.FeatureCount." +
          OptimizationTargetToHistogramVariant(segment_id),
      count);
}

void RecordModelDeliveryMetadataValidation(
    OptimizationTarget segment_id,
    bool processed,
    metadata_utils::ValidationResult validation_result) {
  // Should map to ValidationPhase variant string in
  // //tools/metrics/histograms/metadata/segmentation_platform/histograms.xml.
  std::string validation_phase = processed ? "Processed" : "Incoming";
  base::UmaHistogramEnumeration(
      "SegmentationPlatform.ModelDelivery.Metadata.Validation." +
          validation_phase + "." +
          OptimizationTargetToHistogramVariant(segment_id),
      validation_result);
}

void RecordModelDeliveryReceived(OptimizationTarget segment_id) {
  UMA_HISTOGRAM_ENUMERATION("SegmentationPlatform.ModelDelivery.Received",
                            OptimizationTargetToSegmentationModel(segment_id));
}

void RecordModelDeliverySaveResult(OptimizationTarget segment_id,
                                   bool success) {
  base::UmaHistogramBoolean(
      "SegmentationPlatform.ModelDelivery.SaveResult." +
          OptimizationTargetToHistogramVariant(segment_id),
      success);
}

void RecordModelDeliverySegmentIdMatches(OptimizationTarget segment_id,
                                         bool matches) {
  base::UmaHistogramBoolean(
      "SegmentationPlatform.ModelDelivery.SegmentIdMatches." +
          OptimizationTargetToHistogramVariant(segment_id),
      matches);
}

void RecordModelExecutionDurationFeatureProcessing(
    OptimizationTarget segment_id,
    base::TimeDelta duration) {
  base::UmaHistogramTimes(
      "SegmentationPlatform.ModelExecution.Duration.FeatureProcessing." +
          OptimizationTargetToHistogramVariant(segment_id),
      duration);
}

void RecordModelExecutionDurationModel(OptimizationTarget segment_id,
                                       bool success,
                                       base::TimeDelta duration) {
  ModelExecutionStatus status = success ? ModelExecutionStatus::kSuccess
                                        : ModelExecutionStatus::kExecutionError;
  base::UmaHistogramTimes(
      "SegmentationPlatform.ModelExecution.Duration.Model." +
          OptimizationTargetToHistogramVariant(segment_id) + "." +
          ModelExecutionStatusToHistogramVariant(status),
      duration);
}

void RecordModelExecutionDurationTotal(OptimizationTarget segment_id,
                                       ModelExecutionStatus status,
                                       base::TimeDelta duration) {
  base::UmaHistogramTimes(
      "SegmentationPlatform.ModelExecution.Duration.Total." +
          OptimizationTargetToHistogramVariant(segment_id) + "." +
          ModelExecutionStatusToHistogramVariant(status),
      duration);
}

void RecordModelExecutionResult(OptimizationTarget segment_id, float result) {
  base::UmaHistogramPercentage(
      "SegmentationPlatform.ModelExecution.Result." +
          OptimizationTargetToHistogramVariant(segment_id),
      result * 100);
}

void RecordModelExecutionSaveResult(OptimizationTarget segment_id,
                                    bool success) {
  base::UmaHistogramBoolean(
      "SegmentationPlatform.ModelExecution.SaveResult." +
          OptimizationTargetToHistogramVariant(segment_id),
      success);
}

void RecordModelExecutionStatus(OptimizationTarget segment_id,
                                ModelExecutionStatus status) {
  base::UmaHistogramEnumeration(
      "SegmentationPlatform.ModelExecution.Status." +
          OptimizationTargetToHistogramVariant(segment_id),
      status);
}

void RecordModelExecutionZeroValuePercent(OptimizationTarget segment_id,
                                          const std::vector<float>& tensor) {
  base::UmaHistogramPercentage(
      "SegmentationPlatform.ModelExecution.ZeroValuePercent." +
          OptimizationTargetToHistogramVariant(segment_id),
      ZeroValueFraction(tensor) * 100);
}

void RecordSignalDatabaseGetSamplesDatabaseEntryCount(size_t count) {
  UMA_HISTOGRAM_COUNTS_1000(
      "SegmentationPlatform.SignalDatabase.GetSamples.DatabaseEntryCount",
      count);
}

void RecordSignalDatabaseGetSamplesResult(bool success) {
  UMA_HISTOGRAM_BOOLEAN("SegmentationPlatform.SignalDatabase.GetSamples.Result",
                        success);
}

void RecordSignalDatabaseGetSamplesSampleCount(size_t count) {
  UMA_HISTOGRAM_COUNTS_10000(
      "SegmentationPlatform.SignalDatabase.GetSamples.SampleCount", count);
}

void RecordSignalsListeningCount(
    const std::set<uint64_t>& user_actions,
    const std::set<std::pair<std::string, proto::SignalType>>& histograms) {
  uint64_t user_action_count = user_actions.size();
  uint64_t histogram_enum_count = 0;
  uint64_t histogram_value_count = 0;
  for (auto& s : histograms) {
    if (s.second == proto::SignalType::HISTOGRAM_ENUM)
      ++histogram_enum_count;
    if (s.second == proto::SignalType::HISTOGRAM_VALUE)
      ++histogram_value_count;
  }

  base::UmaHistogramCounts1000(
      "SegmentationPlatform.Signals.ListeningCount." +
          SignalTypeToHistogramVariant(proto::SignalType::USER_ACTION),
      user_action_count);
  base::UmaHistogramCounts1000(
      "SegmentationPlatform.Signals.ListeningCount." +
          SignalTypeToHistogramVariant(proto::SignalType::HISTOGRAM_ENUM),
      histogram_enum_count);
  base::UmaHistogramCounts1000(
      "SegmentationPlatform.Signals.ListeningCount." +
          SignalTypeToHistogramVariant(proto::SignalType::HISTOGRAM_VALUE),
      histogram_value_count);
}

}  // namespace stats
}  // namespace segmentation_platform
