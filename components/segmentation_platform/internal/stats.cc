// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/stats.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/proto/types.pb.h"
#include "components/segmentation_platform/public/config.h"

namespace segmentation_platform::stats {
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
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_DUMMY:
      return "Dummy";
    case OptimizationTarget::
        OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID:
      return "ChromeStartAndroid";
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_QUERY_TILES:
      return "QueryTiles";
    case OptimizationTarget::
        OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT:
      return "ChromeLowUserEngagement";
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
  kDummy = 10,
  kChromeStartAndroid = 11,
  kQueryTiles = 12,
  kChromeLowUserEngagement = 16,
  kMaxValue = kChromeLowUserEngagement,
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

bool IsBooleanSegment(const std::string& segmentation_key) {
  // Please keep in sync with BooleanModel variant in
  // //tools/metrics/histograms/metadata/segmentation_platform/histograms.xml.
  return segmentation_key == kChromeStartAndroidSegmentationKey ||
         segmentation_key == kQueryTilesSegmentationKey ||
         segmentation_key == kChromeLowUserEngagementSegmentationKey;
}

BooleanSegmentSwitch GetBooleanSegmentSwitch(
    OptimizationTarget new_selection,
    OptimizationTarget previous_selection) {
  if (new_selection != OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN &&
      previous_selection == OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN) {
    return BooleanSegmentSwitch::kNoneToEnabled;
  } else if (new_selection == OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN &&
             previous_selection !=
                 OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN) {
    return BooleanSegmentSwitch::kEnabledToNone;
  }
  return BooleanSegmentSwitch::kUnknown;
}

AdaptiveToolbarSegmentSwitch GetAdaptiveToolbarSegmentSwitch(
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
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_DUMMY:
      return SegmentationModel::kDummy;
    case OptimizationTarget::
        OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID:
      return SegmentationModel::kChromeStartAndroid;
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_QUERY_TILES:
      return SegmentationModel::kQueryTiles;
    case OptimizationTarget::
        OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT:
      return SegmentationModel::kChromeLowUserEngagement;
    default:
      return SegmentationModel::kUnknown;
  }
}

// Should map to ModelExecutionStatus variant string in
// //tools/metrics/histograms/metadata/segmentation_platform/histograms.xml.
absl::optional<base::StringPiece> ModelExecutionStatusToHistogramVariant(
    ModelExecutionStatus status) {
  switch (status) {
    case ModelExecutionStatus::kSuccess:
      return "Success";
    case ModelExecutionStatus::kExecutionError:
      return "ExecutionError";

    // Only record duration histograms when tflite model is executed. These
    // cases mean the execution was skipped.
    case ModelExecutionStatus::kSkippedInvalidMetadata:
    case ModelExecutionStatus::kSkippedModelNotReady:
    case ModelExecutionStatus::kSkippedHasFreshResults:
    case ModelExecutionStatus::kSkippedNotEnoughSignals:
    case ModelExecutionStatus::kSkippedResultNotExpired:
    case ModelExecutionStatus::kFailedToSaveResultAfterSuccess:
      return absl::nullopt;
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

const char* SegmentationKeyToUmaName(const std::string& segmentation_key) {
  // Please keep in sync with SegmentationKey variant in
  // //tools/metrics/histograms/metadata/segmentation_platform/histograms.xml.
  if (segmentation_key == kAdaptiveToolbarSegmentationKey) {
    return "AdaptiveToolbar";
  } else if (segmentation_key == kDummySegmentationKey) {
    return "DummyFeature";
  } else if (segmentation_key == kChromeStartAndroidSegmentationKey) {
    return "ChromeStartAndroid";
  } else if (segmentation_key == kQueryTilesSegmentationKey) {
    return "QueryTiles";
  } else if (segmentation_key == kChromeLowUserEngagementSegmentationKey) {
    return "ChromeLowUserEngagement";
  } else if (base::StartsWith(segmentation_key, "test_key")) {
    return "TestKey";
  }
  NOTREACHED();
  return "Unknown";
}

}  // namespace

void RecordModelScore(OptimizationTarget segment_id, float score) {
  // Special case adaptive toolbar models since it already has histograms being
  // recorded and updating names will affect current work.
  switch (segment_id) {
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
      base::UmaHistogramPercentage(
          "SegmentationPlatform.AdaptiveToolbar.ModelScore." +
              OptimizationTargetToHistogramVariant(segment_id),
          score * 100);
      break;
    default:
      break;
  }

  switch (segment_id) {
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_DUMMY:
    case OptimizationTarget::
        OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID:
    case OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_QUERY_TILES:
    case OptimizationTarget::
        OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT:
      // Assumes all models return score between 0 and 1. This is true for all
      // the models we have currently.
      base::UmaHistogramPercentage(
          "SegmentationPlatform.ModelScore." +
              OptimizationTargetToHistogramVariant(segment_id),
          score * 100);
      break;
    default:
      break;
  }
}

void RecordSegmentSelectionComputed(
    const std::string& segmentation_key,
    OptimizationTarget new_selection,
    absl::optional<OptimizationTarget> previous_selection) {
  // Special case adaptive toolbar since it already has histograms being
  // recorded and updating names will affect current work.
  if (segmentation_key == kAdaptiveToolbarSegmentationKey) {
    base::UmaHistogramEnumeration(
        "SegmentationPlatform.AdaptiveToolbar.SegmentSelection.Computed",
        OptimizationTargetToAdaptiveToolbarButtonVariant(new_selection));
  }
  std::string computed_hist = base::StrCat(
      {"SegmentationPlatform.", SegmentationKeyToUmaName(segmentation_key),
       ".SegmentSelection.Computed2"});
  base::UmaHistogramEnumeration(
      computed_hist, OptimizationTargetToSegmentationModel(new_selection));

  OptimizationTarget prev_segment =
      previous_selection.has_value()
          ? previous_selection.value()
          : OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN;

  if (prev_segment == new_selection)
    return;

  std::string switched_hist = base::StrCat(
      {"SegmentationPlatform.", SegmentationKeyToUmaName(segmentation_key),
       ".SegmentSwitched"});
  if (segmentation_key == kAdaptiveToolbarSegmentationKey) {
    base::UmaHistogramEnumeration(
        switched_hist,
        GetAdaptiveToolbarSegmentSwitch(new_selection, prev_segment));
  } else if (IsBooleanSegment(segmentation_key)) {
    base::UmaHistogramEnumeration(
        switched_hist, GetBooleanSegmentSwitch(new_selection, prev_segment));
  }
  // Do not record switched histogram for all keys by default, the client needs
  // to write custom logic for other kinds of segments.
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
  absl::optional<base::StringPiece> status_variant =
      ModelExecutionStatusToHistogramVariant(status);
  if (!status_variant)
    return;
  base::UmaHistogramTimes(
      base::StrCat({"SegmentationPlatform.ModelExecution.Duration.Model.",
                    OptimizationTargetToHistogramVariant(segment_id), ".",
                    *status_variant}),
      duration);
}

void RecordModelExecutionDurationTotal(OptimizationTarget segment_id,
                                       ModelExecutionStatus status,
                                       base::TimeDelta duration) {
  absl::optional<base::StringPiece> status_variant =
      ModelExecutionStatusToHistogramVariant(status);
  if (!status_variant)
    return;
  base::UmaHistogramTimes(
      base::StrCat({"SegmentationPlatform.ModelExecution.Duration.Total.",
                    OptimizationTargetToHistogramVariant(segment_id), ".",
                    *status_variant}),
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
                                bool default_provider,
                                ModelExecutionStatus status) {
  if (!default_provider) {
    base::UmaHistogramEnumeration(
        "SegmentationPlatform.ModelExecution.Status." +
            OptimizationTargetToHistogramVariant(segment_id),
        status);
  } else {
    base::UmaHistogramEnumeration(
        "SegmentationPlatform.ModelExecution.DefaultProvider.Status." +
            OptimizationTargetToHistogramVariant(segment_id),
        status);
  }
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

void RecordSegmentSelectionFailure(const std::string& segmentation_key,
                                   SegmentationSelectionFailureReason reason) {
  base::UmaHistogramEnumeration(
      base::StrCat({"SegmentationPlatform.SelectionFailedReason.",
                    SegmentationKeyToUmaName(segmentation_key)}),
      reason);
}

void RecordModelAvailability(OptimizationTarget segment_id,
                             SegmentationModelAvailability availability) {
  base::UmaHistogramEnumeration(
      "SegmentationPlatform.ModelAvailability." +
          OptimizationTargetToHistogramVariant(segment_id),
      availability);
}

void RecordTooManyInputTensors(int tensor_size) {
  UMA_HISTOGRAM_COUNTS_100(
      "SegmentationPlatform.StructuredMetrics.TooManyTensors.Count",
      tensor_size);
}

void RecordTrainingDataCollectionEvent(OptimizationTarget segment_id,
                                       TrainingDataCollectionEvent event) {
  base::UmaHistogramEnumeration(
      "SegmentationPlatform.TrainingDataCollectionEvents." +
          OptimizationTargetToHistogramVariant(segment_id),
      event);
}

}  // namespace segmentation_platform::stats
