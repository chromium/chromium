// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/stats.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/thread_pool.h"
#include "components/segmentation_platform/internal/post_processor/post_processor.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/output_config.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"

namespace segmentation_platform::stats {
namespace {

// Keep in sync with AdaptiveToolbarButtonVariant in enums.xml.
enum class AdaptiveToolbarButtonVariant {
  kUnknown = 0,
  kNone = 1,
  kNewTab = 2,
  kShare = 3,
  kVoice = 4,
  kMaxValue = kVoice,
};

// It should only used for legacy models without descriptors of return type in
// the metadata.
proto::SegmentationModelMetadata::OutputDescription
GetOptimizationTargetOutputDescription(SegmentId segment_id) {
  switch (segment_id) {
    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_DUMMY:
    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID:
    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT:
    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER:
    case SegmentId::OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING:
    case SegmentId::OPTIMIZATION_TARGET_WEB_APP_INSTALLATION_PROMO:
    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_COMPOSE_PROMOTION:
      return proto::SegmentationModelMetadata::RETURN_TYPE_PROBABILITY;
    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER:
    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_TABLET_PRODUCTIVITY_USER:
    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_IOS_MODULE_RANKER:
    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_ANDROID_HOME_MODULE_RANKER:
      return proto::SegmentationModelMetadata::RETURN_TYPE_MULTISEGMENT;
    default:
      return proto::SegmentationModelMetadata::UNKNOWN_RETURN_TYPE;
  }
}

AdaptiveToolbarButtonVariant OptimizationTargetToAdaptiveToolbarButtonVariant(
    SegmentId segment_id) {
  switch (segment_id) {
    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
      return AdaptiveToolbarButtonVariant::kNewTab;
    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
      return AdaptiveToolbarButtonVariant::kShare;
    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
      return AdaptiveToolbarButtonVariant::kVoice;
    case SegmentId::OPTIMIZATION_TARGET_UNKNOWN:
      return AdaptiveToolbarButtonVariant::kNone;
    default:
      NOTREACHED_IN_MIGRATION();
      return AdaptiveToolbarButtonVariant::kUnknown;
  }
}

BooleanSegmentSwitch GetBooleanSegmentSwitch(SegmentId new_selection,
                                             SegmentId previous_selection) {
  if (new_selection != SegmentId::OPTIMIZATION_TARGET_UNKNOWN &&
      previous_selection == SegmentId::OPTIMIZATION_TARGET_UNKNOWN) {
    return BooleanSegmentSwitch::kNoneToEnabled;
  } else if (new_selection == SegmentId::OPTIMIZATION_TARGET_UNKNOWN &&
             previous_selection != SegmentId::OPTIMIZATION_TARGET_UNKNOWN) {
    return BooleanSegmentSwitch::kEnabledToNone;
  }
  return BooleanSegmentSwitch::kUnknown;
}

AdaptiveToolbarSegmentSwitch GetAdaptiveToolbarSegmentSwitch(
    SegmentId new_selection,
    SegmentId previous_selection) {
  switch (previous_selection) {
    case SegmentId::OPTIMIZATION_TARGET_UNKNOWN:
      switch (new_selection) {
        case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
          return AdaptiveToolbarSegmentSwitch::kNoneToNewTab;
        case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
          return AdaptiveToolbarSegmentSwitch::kNoneToShare;
        case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
          return AdaptiveToolbarSegmentSwitch::kNoneToVoice;
        default:
          NOTREACHED_IN_MIGRATION();
          return AdaptiveToolbarSegmentSwitch::kUnknown;
      }

    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
      switch (new_selection) {
        case SegmentId::OPTIMIZATION_TARGET_UNKNOWN:
          return AdaptiveToolbarSegmentSwitch::kNewTabToNone;
        case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
          return AdaptiveToolbarSegmentSwitch::kNewTabToShare;
        case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
          return AdaptiveToolbarSegmentSwitch::kNewTabToVoice;
        default:
          NOTREACHED_IN_MIGRATION();
          return AdaptiveToolbarSegmentSwitch::kUnknown;
      }

    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
      switch (new_selection) {
        case SegmentId::OPTIMIZATION_TARGET_UNKNOWN:
          return AdaptiveToolbarSegmentSwitch::kShareToNone;
        case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
          return AdaptiveToolbarSegmentSwitch::kShareToNewTab;
        case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
          return AdaptiveToolbarSegmentSwitch::kShareToVoice;
        default:
          NOTREACHED_IN_MIGRATION();
          return AdaptiveToolbarSegmentSwitch::kUnknown;
      }

    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
      switch (new_selection) {
        case SegmentId::OPTIMIZATION_TARGET_UNKNOWN:
          return AdaptiveToolbarSegmentSwitch::kVoiceToNone;
        case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
          return AdaptiveToolbarSegmentSwitch::kVoiceToNewTab;
        case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
          return AdaptiveToolbarSegmentSwitch::kVoiceToShare;
        default:
          NOTREACHED_IN_MIGRATION();
          return AdaptiveToolbarSegmentSwitch::kUnknown;
      }

    default:
      NOTREACHED_IN_MIGRATION();
      return AdaptiveToolbarSegmentSwitch::kUnknown;
  }
}

// Should map to ModelExecutionStatus variant string in
// //tools/metrics/histograms/metadata/segmentation_platform/histograms.xml.
std::optional<std::string_view> ModelExecutionStatusToHistogramVariant(
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
      return std::nullopt;
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
      NOTREACHED_IN_MIGRATION();
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

// For server models to keep the same name as before, empty string is returned.
std::string GetModelSourceAsString(proto::ModelSource model_source) {
  // Should map to ModelSource variant string in
  // //tools/metrics/histograms/metadata/segmentation_platform/histograms.xml.
  return (model_source == proto::DEFAULT_MODEL_SOURCE ? "Default" : "");
}
}  // namespace

void RecordModelUpdateTimeDifference(SegmentId segment_id,
                                     int64_t model_update_time) {
  // |model_update_time| might be empty for data persisted before M101.
  if (model_update_time) {
    base::Time model_updated_time = base::Time::FromDeltaSinceWindowsEpoch(
        base::Seconds(model_update_time));
    base::UmaHistogramCounts1000(
        "SegmentationPlatform.Init.ModelUpdatedTimeDifferenceInDays." +
            SegmentIdToHistogramVariant(segment_id),
        (base::Time::Now() - model_updated_time).InDays());
  }
}

void RecordSegmentSelectionComputed(
    const Config& config,
    SegmentId new_selection,
    std::optional<SegmentId> previous_selection) {
  // Special case adaptive toolbar since it already has histograms being
  // recorded and updating names will affect current work.
  if (config.segmentation_key == kAdaptiveToolbarSegmentationKey) {
    base::UmaHistogramEnumeration(
        "SegmentationPlatform.AdaptiveToolbar.SegmentSelection.Computed",
        OptimizationTargetToAdaptiveToolbarButtonVariant(new_selection));
  }
  std::string computed_hist =
      base::StrCat({"SegmentationPlatform.", config.segmentation_uma_name,
                    ".SegmentSelection.Computed2"});
  base::UmaHistogramSparse(computed_hist, new_selection);

  SegmentId prev_segment = previous_selection.has_value()
                               ? previous_selection.value()
                               : SegmentId::OPTIMIZATION_TARGET_UNKNOWN;

  if (prev_segment == new_selection || !config.auto_execute_and_cache) {
    return;
  }

  std::string switched_hist =
      base::StrCat({"SegmentationPlatform.", config.segmentation_uma_name,
                    ".SegmentSwitched"});
  if (config.segmentation_key == kAdaptiveToolbarSegmentationKey) {
    base::UmaHistogramEnumeration(
        switched_hist,
        GetAdaptiveToolbarSegmentSwitch(new_selection, prev_segment));
  } else if (config.is_boolean_segment) {
    base::UmaHistogramEnumeration(
        switched_hist, GetBooleanSegmentSwitch(new_selection, prev_segment));
  }
  // Do not record switched histogram for all keys by default, the client needs
  // to write custom logic for other kinds of segments.
}

void RecordClassificationResultComputed(
    const Config& config,
    const proto::PredictionResult& new_result) {
  if (new_result.output_config().predictor().PredictorType_case() ==
      proto::Predictor::kGenericPredictor) {
    return;
  }
  PostProcessor post_processor;
  int new_result_top_label = post_processor.GetIndexOfTopLabel(new_result);
  std::string computed_hist =
      base::StrCat({"SegmentationPlatform.", config.segmentation_uma_name,
                    ".PostProcessing.TopLabel.Computed"});
  base::UmaHistogramSparse(computed_hist, new_result_top_label);
}

void RecordClassificationResultUpdated(
    const Config& config,
    const proto::PredictionResult* old_result,
    const proto::PredictionResult& new_result) {
  PostProcessor post_processor;
  int new_result_top_label = post_processor.GetIndexOfTopLabel(new_result);
  int old_result_top_label =
      old_result ? post_processor.GetIndexOfTopLabel(*old_result) : -2;
  if (old_result_top_label == new_result_top_label) {
    return;
  }

  std::string switched_hist =
      base::StrCat({"SegmentationPlatform.", config.segmentation_uma_name,
                    ".PostProcessing.TopLabel.Switched"});
  // There is no easy way to record this metric for label switch. So we encode
  // it as follows: Multiply the index value of the old value by 100 and add the
  // new index value. Note, there might be negative integers, but regardless
  // this will generate a unique value for each type of label switch.
  // For example, for a 3-label case, any transition will look like
  // none -> label 0 : -200
  // none -> label 1 : -199
  // none -> label 2 : -198
  // label 0 -> none : -2
  // label 0 -> label 1 : 1
  // label 0 -> label 2 : 2
  // label 1 -> none : 98
  // label 1 -> label 0 : 100
  // label 1 -> label 2 : 102
  // label 2 -> none : 198
  // label 2 -> label 0 : 200
  // label 2 -> label 1 : 201

  int switch_value = old_result_top_label * 100 + new_result_top_label;
  base::UmaHistogramSparse(switched_hist, switch_value);
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

void RecordModelDeliveryHasMetadata(SegmentId segment_id, bool has_metadata) {
  base::UmaHistogramBoolean("SegmentationPlatform.ModelDelivery.HasMetadata." +
                                SegmentIdToHistogramVariant(segment_id),
                            has_metadata);
}

void RecordModelDeliveryMetadataFeatureCount(SegmentId segment_id,
                                             ModelSource model_source,
                                             size_t count) {
  base::UmaHistogramCounts1000("SegmentationPlatform." +
                                   GetModelSourceAsString(model_source) +
                                   "ModelDelivery.Metadata.FeatureCount." +
                                   SegmentIdToHistogramVariant(segment_id),
                               count);
}

void RecordModelDeliveryMetadataValidation(
    SegmentId segment_id,
    proto::ModelSource model_source,
    bool processed,
    metadata_utils::ValidationResult validation_result) {
  // Should map to ValidationPhase variant string in
  // //tools/metrics/histograms/metadata/segmentation_platform/histograms.xml.
  std::string validation_phase = processed ? "Processed" : "Incoming";
  base::UmaHistogramEnumeration(
      "SegmentationPlatform." + GetModelSourceAsString(model_source) +
          "ModelDelivery.Metadata.Validation." + validation_phase + "." +
          SegmentIdToHistogramVariant(segment_id),
      validation_result);
}

void RecordModelDeliveryReceived(SegmentId segment_id,
                                 proto::ModelSource model_source) {
  base::UmaHistogramSparse("SegmentationPlatform." +
                               GetModelSourceAsString(model_source) +
                               "ModelDelivery.Received",
                           segment_id);
}

void RecordModelDeliverySaveResult(SegmentId segment_id,
                                   proto::ModelSource model_source,
                                   bool success) {
  base::UmaHistogramBoolean(
      "SegmentationPlatform." + GetModelSourceAsString(model_source) +
          "ModelDelivery.SaveResult." + SegmentIdToHistogramVariant(segment_id),
      success);
}

void RecordModelDeliveryDeleteResult(SegmentId segment_id,
                                     proto::ModelSource model_source,
                                     bool success) {
  base::UmaHistogramBoolean("SegmentationPlatform." +
                                GetModelSourceAsString(model_source) +
                                "ModelDelivery.DeleteResult." +
                                SegmentIdToHistogramVariant(segment_id),
                            success);
}

void RecordModelDeliverySegmentIdMatches(SegmentId segment_id,
                                         proto::ModelSource model_source,
                                         bool matches) {
  base::UmaHistogramBoolean("SegmentationPlatform." +
                                GetModelSourceAsString(model_source) +
                                "ModelDelivery.SegmentIdMatches." +
                                SegmentIdToHistogramVariant(segment_id),
                            matches);
}

void RecordModelExecutionDurationFeatureProcessing(SegmentId segment_id,
                                                   base::TimeDelta duration) {
  base::UmaHistogramTimes(
      "SegmentationPlatform.ModelExecution.Duration.FeatureProcessing." +
          SegmentIdToHistogramVariant(segment_id),
      duration);
}

void RecordModelExecutionDurationModel(SegmentId segment_id,
                                       bool success,
                                       base::TimeDelta duration) {
  ModelExecutionStatus status = success ? ModelExecutionStatus::kSuccess
                                        : ModelExecutionStatus::kExecutionError;
  std::optional<std::string_view> status_variant =
      ModelExecutionStatusToHistogramVariant(status);
  if (!status_variant)
    return;
  base::UmaHistogramTimes(
      base::StrCat({"SegmentationPlatform.ModelExecution.Duration.Model.",
                    SegmentIdToHistogramVariant(segment_id), ".",
                    *status_variant}),
      duration);
}

void RecordModelExecutionDurationTotal(SegmentId segment_id,
                                       ModelExecutionStatus status,
                                       base::TimeDelta duration) {
  std::optional<std::string_view> status_variant =
      ModelExecutionStatusToHistogramVariant(status);
  if (!status_variant)
    return;
  base::UmaHistogramTimes(
      base::StrCat({"SegmentationPlatform.ModelExecution.Duration.Total.",
                    SegmentIdToHistogramVariant(segment_id), ".",
                    *status_variant}),
      duration);
}

void RecordClassificationRequestTotalDuration(const Config& config,
                                              base::TimeDelta duration) {
  std::string histogram_name =
      base::StrCat({"SegmentationPlatform.ClassificationRequest.TotalDuration.",
                    config.segmentation_uma_name});
  base::UmaHistogramTimes(histogram_name, duration);
}

void RecordOnDemandSegmentSelectionDuration(
    const Config& config,
    const SegmentSelectionResult& result,
    base::TimeDelta duration) {
  std::string histogram_prefix =
      base::StrCat({"SegmentationPlatform.SegmentSelectionOnDemand.Duration.",
                    config.segmentation_uma_name});
  base::UmaHistogramTimes(base::StrCat({histogram_prefix, "Any"}), duration);
}

void RecordModelExecutionResult(
    SegmentId segment_id,
    float result,
    proto::SegmentationModelMetadata::OutputDescription return_type) {
  if (return_type == proto::SegmentationModelMetadata::UNKNOWN_RETURN_TYPE) {
    return_type = GetOptimizationTargetOutputDescription(segment_id);
  }
  if (return_type ==
      proto::SegmentationModelMetadata::RETURN_TYPE_MULTISEGMENT) {
    // This type of model return score between 0 and 100.
    base::UmaHistogramPercentage("SegmentationPlatform.ModelExecution.Result." +
                                     SegmentIdToHistogramVariant(segment_id),
                                 base::ClampRound(result));
    return;
  } else if (return_type ==
             proto::SegmentationModelMetadata::RETURN_TYPE_INTEGER) {
    // This type of model return an unbound float score.
    base::UmaHistogramPercentage("SegmentationPlatform.ModelExecution.Result." +
                                     SegmentIdToHistogramVariant(segment_id),
                                 static_cast<int>(result));
    return;
  }
  // All other models type return score between 0 and 1.
  base::UmaHistogramPercentage("SegmentationPlatform.ModelExecution.Result." +
                                   SegmentIdToHistogramVariant(segment_id),
                               result * 100);
}

void RecordModelExecutionResult(SegmentId segment_id,
                                const ModelProvider::Response& result,
                                proto::OutputConfig output_config) {
  // Only for binary and multi-class classifier, we treat the score as a
  // probability score and multiply by 100. For others, it's kept as is.
  bool is_probability_score = false;
  switch (output_config.predictor().PredictorType_case()) {
    case proto::Predictor::kBinaryClassifier:
      [[fallthrough]];
    case proto::Predictor::kMultiClassClassifier:
      is_probability_score = true;
      break;
    case proto::Predictor::kBinnedClassifier:
      [[fallthrough]];
    case proto::Predictor::kRegressor:
      [[fallthrough]];
    case proto::Predictor::kGenericPredictor:
      is_probability_score = false;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  for (size_t i = 0; i < result.size(); i++) {
    std::string histogram_name = "SegmentationPlatform.ModelExecution.Result." +
                                 base::NumberToString(i) + "." +
                                 SegmentIdToHistogramVariant(segment_id);
    int scaled_model_score = is_probability_score ? result[i] * 100 : result[i];
    base::UmaHistogramPercentage(histogram_name, scaled_model_score);
  }
}

void RecordModelExecutionSaveResult(SegmentId segment_id, bool success) {
  base::UmaHistogramBoolean("SegmentationPlatform.ModelExecution.SaveResult." +
                                SegmentIdToHistogramVariant(segment_id),
                            success);
}

void RecordModelExecutionStatus(SegmentId segment_id,
                                bool default_provider,
                                ModelExecutionStatus status) {
  if (!default_provider) {
    base::UmaHistogramEnumeration(
        "SegmentationPlatform.ModelExecution.Status." +
            SegmentIdToHistogramVariant(segment_id),
        status);
  } else {
    base::UmaHistogramEnumeration(
        "SegmentationPlatform.ModelExecution.DefaultProvider.Status." +
            SegmentIdToHistogramVariant(segment_id),
        status);
  }
}

void RecordModelExecutionZeroValuePercent(SegmentId segment_id,
                                          const std::vector<float>& tensor) {
  BackgroundUmaRecorder::GetInstance().AddMetric(base::BindOnce(
      [](const std::string& name, int value) {
        base::UmaHistogramPercentage(name, value);
      },
      "SegmentationPlatform.ModelExecution.ZeroValuePercent." +
          SegmentIdToHistogramVariant(segment_id),
      ZeroValueFraction(tensor) * 100));
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

void RecordSegmentInfoDatabaseUpdateEntriesResult(SegmentId segment_id,
                                                  bool success) {
  base::UmaHistogramBoolean(
      "SegmentationPlatform.SegmentInfoDatabase.ProtoDBUpdateResult." +
          SegmentIdToHistogramVariant(segment_id),
      success);
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

void RecordSegmentSelectionFailure(const Config& config,
                                   SegmentationSelectionFailureReason reason) {
  base::UmaHistogramEnumeration(
      base::StrCat({"SegmentationPlatform.SelectionFailedReason.",
                    config.segmentation_uma_name}),
      reason);
}

std::string FeatureProcessingErrorToString(FeatureProcessingError error) {
  switch (error) {
    case FeatureProcessingError::kUkmEngineDisabled:
      return "UkmEngineDisabled";
    case FeatureProcessingError::kUmaValidationError:
      return "UmaValidationError";
    case FeatureProcessingError::kSqlValidationError:
      return "SqlValidationError";
    case FeatureProcessingError::kCustomInputError:
      return "CustomInputError";
    case FeatureProcessingError::kSqlBindValuesError:
      return "SqlBindValuesError";
    case FeatureProcessingError::kSqlQueryRunError:
      return "SqlQueryRunError";
    case FeatureProcessingError::kResultTensorError:
      return "ResultTensorError";
    default:
      return "Other";
  }
}

void RecordFeatureProcessingError(SegmentId segment_id,
                                  FeatureProcessingError error) {
  base::UmaHistogramEnumeration(
      "SegmentationPlatform.FeatureProcessing.Error." +
          SegmentIdToHistogramVariant(segment_id),
      error);
}

void RecordModelAvailability(SegmentId segment_id,
                             SegmentationModelAvailability availability) {
  base::UmaHistogramEnumeration("SegmentationPlatform.ModelAvailability." +
                                    SegmentIdToHistogramVariant(segment_id),
                                availability);
}

void RecordTooManyInputTensors(int tensor_size) {
  UMA_HISTOGRAM_COUNTS_100(
      "SegmentationPlatform.StructuredMetrics.TooManyTensors.Count",
      tensor_size);
}

std::string TrainingDataCollectionEventToErrorMsg(
    TrainingDataCollectionEvent event) {
  switch (event) {
    case TrainingDataCollectionEvent::kImmediateCollectionStart:
      return "Immediate Collection Start";
    case TrainingDataCollectionEvent::kImmediateCollectionSuccess:
      return "Immediate Collection Success";
    case TrainingDataCollectionEvent::kModelInfoMissing:
      return "Model Info Missing";
    case TrainingDataCollectionEvent::kMetadataValidationFailed:
      return "Metadata Validation Failed";
    case TrainingDataCollectionEvent::kGetInputTensorsFailed:
      return "Get Input Tensors Failed";
    case TrainingDataCollectionEvent::kNotEnoughCollectionTime:
      return "Not Enough Collection Time";
    case TrainingDataCollectionEvent::kUkmReportingFailed:
      return "UKM Reporting Failed";
    case TrainingDataCollectionEvent::kPartialDataNotAllowed:
      return "Partial Data Not Allowed";
    case TrainingDataCollectionEvent::kContinousCollectionStart:
      return "Continuous Collection Start";
    case TrainingDataCollectionEvent::kContinousCollectionSuccess:
      return "Continuous Collection Success";
    case TrainingDataCollectionEvent::kCollectAndStoreInputsSuccess:
      return "Collect and Store Inputs Success";
    case TrainingDataCollectionEvent::kObservationTimeReached:
      return "Observation Time Reached";
    case TrainingDataCollectionEvent::kDelayedTaskPosted:
      return "Delayed Task Posted";
    case TrainingDataCollectionEvent::kImmediateObservationPosted:
      return "Immediate Observation Posted";
    case TrainingDataCollectionEvent::kWaitingForNonDelayedTrigger:
      return "Waiting for Non Delayed Trigger";
    case TrainingDataCollectionEvent::kHistogramTriggerHit:
      return "Histogram Trigger Hit";
    case TrainingDataCollectionEvent::kNoSegmentInfo:
      return "No Segment Info";
    case TrainingDataCollectionEvent::kDisallowedForRecording:
      return "Disallowed for Recording";
    case TrainingDataCollectionEvent::kObservationDisallowed:
      return "Observation Disallowed";
    case TrainingDataCollectionEvent::kTrainingDataMissing:
      return "Training Data Missing";
    case TrainingDataCollectionEvent::kOnDecisionTimeTypeMistmatch:
      return "On Decision Time Type Mismatch";
    case TrainingDataCollectionEvent::kDelayTriggerSampled:
      return "Delay Trigger Sampled";
    case TrainingDataCollectionEvent::
        kContinousExactPredictionTimeCollectionStart:
      return "Continuous Exact Prediction Time Collection Start";
    case TrainingDataCollectionEvent::
        kContinousExactPredictionTimeCollectionSuccess:
      return "Continuous Exact Prediction Time Collection Success";
    default:
      return "";
  }
}

void RecordTrainingDataCollectionEvent(SegmentId segment_id,
                                       TrainingDataCollectionEvent event) {
  base::UmaHistogramEnumeration(
      "SegmentationPlatform.TrainingDataCollectionEvents." +
          SegmentIdToHistogramVariant(segment_id),
      event);
  VLOG(1) << "Training Data event for "
          << SegmentIdToHistogramVariant(segment_id) << ": "
          << TrainingDataCollectionEventToErrorMsg(event);
}

// This conversion exists because segment selector uses the result state
// differently. TODO(ritikagup): Remove this conversion when selector is
// deleted.
SegmentationSelectionFailureReason GetSuccessOrFailureReason(
    SegmentResultProvider::ResultState result_state) {
  switch (result_state) {
    case SegmentResultProvider::ResultState::kUnknown:
      NOTREACHED_IN_MIGRATION();
      return SegmentationSelectionFailureReason::kMaxValue;
    case SegmentResultProvider::ResultState::kServerModelDatabaseScoreUsed:
      return SegmentationSelectionFailureReason::kServerModelDatabaseScoreUsed;
    case SegmentResultProvider::ResultState::kDefaultModelDatabaseScoreUsed:
      return SegmentationSelectionFailureReason::kDefaultModelDatabaseScoreUsed;
    case SegmentResultProvider::ResultState::kDefaultModelExecutionScoreUsed:
      return SegmentationSelectionFailureReason::
          kDefaultModelExecutionScoreUsed;
    case SegmentResultProvider::ResultState::kServerModelExecutionScoreUsed:
      return SegmentationSelectionFailureReason::kServerModelExecutionScoreUsed;
    case SegmentResultProvider::ResultState::kDefaultModelDatabaseScoreNotReady:
      return SegmentationSelectionFailureReason::
          kDefaultModelDatabaseScoreNotReady;
    case SegmentResultProvider::ResultState::kServerModelDatabaseScoreNotReady:
      return SegmentationSelectionFailureReason::
          kServerModelDatabaseScoreNotReady;
    case SegmentResultProvider::ResultState::
        kDefaultModelSegmentInfoNotAvailable:
      return SegmentationSelectionFailureReason::
          kDefaultModelSegmentInfoNotAvailable;
    case SegmentResultProvider::ResultState::
        kServerModelSegmentInfoNotAvailable:
      return SegmentationSelectionFailureReason::
          kServerModelSegmentInfoNotAvailable;
    case SegmentResultProvider::ResultState::kDefaultModelSignalsNotCollected:
      return SegmentationSelectionFailureReason::
          kDefaultModelSignalsNotCollected;
    case SegmentResultProvider::ResultState::kServerModelSignalsNotCollected:
      return SegmentationSelectionFailureReason::
          kServerModelSignalsNotCollected;
    case SegmentResultProvider::ResultState::kDefaultModelExecutionFailed:
      return SegmentationSelectionFailureReason::kDefaultModelExecutionFailed;
    case SegmentResultProvider::ResultState::kServerModelExecutionFailed:
      return SegmentationSelectionFailureReason::kServerModelExecutionFailed;
  }
}

// static
BackgroundUmaRecorder& BackgroundUmaRecorder::GetInstance() {
  static base::NoDestructor<BackgroundUmaRecorder> instance;
  return *instance;
}

BackgroundUmaRecorder::BackgroundUmaRecorder() = default;

BackgroundUmaRecorder::~BackgroundUmaRecorder() = default;

void BackgroundUmaRecorder::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  if (!bg_task_runner_) {
    // Mark user visible priority so that lock held on bg thread will not block
    // main thread.
    bg_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  }
}

void BackgroundUmaRecorder::InitializeForTesting(
    scoped_refptr<base::SequencedTaskRunner> bg_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  bg_task_runner_ = bg_task_runner;
}

void BackgroundUmaRecorder::AddMetric(base::OnceClosure add_sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
  {
    base::AutoLock l(lock_);
    if (bg_task_runner_) {
      add_samples_.push_back(std::move(add_sample));
      if (!pending_task_) {
        pending_task_ = true;
        bg_task_runner_->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&BackgroundUmaRecorder::FlushSamples,
                           weak_factory_.GetWeakPtr()),
            kMetricsCollectionDelay);
      }
      return;
    }
  }
  std::move(add_sample).Run();
}

void BackgroundUmaRecorder::FlushSamples() {
  std::list<base::OnceClosure> samples;
  {
    base::AutoLock l(lock_);
    samples.swap(add_samples_);
    pending_task_ = false;
  }
  for (auto& it : samples) {
    std::move(it).Run();
  }
}

}  // namespace segmentation_platform::stats
