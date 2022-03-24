// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_STATS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_STATS_H_

#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"
#include "components/segmentation_platform/internal/proto/types.pb.h"
#include "components/segmentation_platform/public/segment_selection_result.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using optimization_guide::proto::OptimizationTarget;

namespace segmentation_platform::stats {

// Keep in sync with AdaptiveToolbarSegmentSwitch in enums.xml.
// Visible for testing.
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

// Keep in sync with SegmentationBooleanSegmentSwitch in enums.xml.
// Visible for testing.
enum class BooleanSegmentSwitch {
  kUnknown = 0,
  kNoneToEnabled = 1,
  kEnabledToNone = 2,
  kMaxValue = kEnabledToNone,
};

// Records the score computed for a given segment.
void RecordModelScore(OptimizationTarget segment_id, float score);

// Records the result of segment selection whenever segment selection is
// computed.
void RecordSegmentSelectionComputed(
    const std::string& segmentation_key,
    OptimizationTarget new_selection,
    absl::optional<OptimizationTarget> previous_selection);

// Database Maintenance metrics.
// Records the number of unique signal identifiers that were successfully
// cleaned up.
void RecordMaintenanceCleanupSignalSuccessCount(size_t count);
// Records the result for each compaction attempt for a particular signal type.
void RecordMaintenanceCompactionResult(proto::SignalType signal_type,
                                       bool success);
// Records the number of signal identifiers that were found that we should aim
// to clean up.
void RecordMaintenanceSignalIdentifierCount(size_t count);

// Model Delivery metrics.
// Records whether any incoming ML model had metadata attached that we were able
// to parse.
void RecordModelDeliveryHasMetadata(OptimizationTarget segment_id,
                                    bool has_metadata);
// Records the number of tensor features an updated ML model has.
void RecordModelDeliveryMetadataFeatureCount(OptimizationTarget segment_id,
                                             size_t count);
// Records the result of validating the metadata of an incoming ML model.
// Recorded before and after it has been merged with the already stored
// metadata.
void RecordModelDeliveryMetadataValidation(
    OptimizationTarget segment_id,
    bool processed,
    metadata_utils::ValidationResult validation_result);
// Record what type of model metadata we received.
void RecordModelDeliveryReceived(OptimizationTarget segment_id);
// Records the result of attempting to save an updated version of the model
// metadata.
void RecordModelDeliverySaveResult(OptimizationTarget segment_id, bool success);
// Records whether the currently stored segment_id matches the incoming
// segment_id, as these are expected to match.
void RecordModelDeliverySegmentIdMatches(OptimizationTarget segment_id,
                                         bool matches);

// Model Execution metrics.
// Records the duration of processing a single ML feature. This only takes into
// account the time it takes to process (aggregate) a feature result, not
// fetching it from the database. It also takes into account filtering any
// enum histograms.
void RecordModelExecutionDurationFeatureProcessing(
    OptimizationTarget segment_id,
    base::TimeDelta duration);
// Records the duration of executing an ML model. This only takes into account
// the time it takes to invoke and wait for a result from the underlying ML
// infrastructure from //components/optimization_guide, and not fetching the
// relevant data from the database.
void RecordModelExecutionDurationModel(OptimizationTarget segment_id,
                                       bool success,
                                       base::TimeDelta duration);
// Records the duration of fetching data for, processing, and executing an ML
// model.
void RecordModelExecutionDurationTotal(OptimizationTarget segment_id,
                                       ModelExecutionStatus status,
                                       base::TimeDelta duration);
// Records the result value after successfully executing an ML model.
void RecordModelExecutionResult(OptimizationTarget segment_id, float result);
// Records whether the result value of of executing an ML model was successfully
// saved.
void RecordModelExecutionSaveResult(OptimizationTarget segment_id,
                                    bool success);
// Records the final execution status for any ML model execution.
void RecordModelExecutionStatus(OptimizationTarget segment_id,
                                bool default_provider,
                                ModelExecutionStatus status);
// Records the percent of features in a tensor that are equal to 0 when the
// segmentation model is executed.
void RecordModelExecutionZeroValuePercent(OptimizationTarget segment_id,
                                          const std::vector<float>& tensor);

// Signal Database metrics.
// Records the number of database entries that were fetched from the database
// during a call to GetSamples. This is not the same as the sample count since
// each database entry can contain multiple samples.
void RecordSignalDatabaseGetSamplesDatabaseEntryCount(size_t count);
// Records the result of fetching data from the database during a call to
// GetSamples.
void RecordSignalDatabaseGetSamplesResult(bool success);
// Records the number of samples that were returned after reading entries from
// the database, during a call to GetSamples. This is not the same as the
// database entry count, since each entry can contain multiple samples.
void RecordSignalDatabaseGetSamplesSampleCount(size_t count);

// Records the number of unique user action and histogram signals that we are
// currently tracking.
void RecordSignalsListeningCount(
    const std::set<uint64_t>& user_actions,
    const std::set<std::pair<std::string, proto::SignalType>>& histograms);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "SegmentationSelectionFailureReason" in
// //tools/metrics/histograms/enums.xml.
enum class SegmentationSelectionFailureReason {
  kPlatformDisabled = 0,
  kSelectionAvailableInPrefs = 1,
  kAtLeastOneSegmentNotReady = 2,
  kAtLeastOneSegmentSignalsNotCollected = 3,
  kSelectionTtlNotExpired = 4,
  kAtLeastOneModelFailedExecution = 5,
  kAtLeastOneModelNeedsMoreSignals = 6,
  kAtLeastOneModelWithInvalidMetadata = 7,
  kFailedToSaveModelResult = 8,
  kInvalidSelectionResultInPrefs = 9,
  kDBInitFailure = 10,
  kAtLeastOneSegmentNotAvailable = 11,
  kAtLeastOneSegmentDefaultSignalNotCollected = 12,
  kAtLeastOneSegmentDefaultExecFailed = 13,
  kAtLeastOneSegmentDefaultMissingMetadata = 14,
  kMaxValue = kAtLeastOneSegmentDefaultMissingMetadata
};

// Records the reason for failure or success to compute a segment selection.
void RecordSegmentSelectionFailure(const std::string& segmentation_key,
                                   SegmentationSelectionFailureReason reason);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "SegmentationModelAvailability" in //tools/metrics/histograms/enums.xml.
enum class SegmentationModelAvailability {
  kModelHandlerCreated = 0,
  kModelAvailable = 1,
  kMetadataInvalid = 2,
  kMaxValue = kMetadataInvalid
};
// Records the availability of segmentation models for each target needed.
void RecordModelAvailability(OptimizationTarget segment_id,
                             SegmentationModelAvailability availability);

// Records the number of input tensor that's causing a failure to upload
// structured metrics.
void RecordTooManyInputTensors(int tensor_size);

// Analytics events for training data collection. Sync with
// SegmentationPlatformTrainingDataCollectionEvent in enums.xml.
enum class TrainingDataCollectionEvent {
  kImmediateCollectionStart = 0,
  kImmediateCollectionSuccess = 1,
  kModelInfoMissing = 2,
  kMetadataValidationFailed = 3,
  kGetInputTensorsFailed = 4,
  kNotEnoughCollectionTime = 5,
  kUkmReportingFailed = 6,
  kMaxValue = kUkmReportingFailed,
};

// Records analytics for training data collection.
void RecordTrainingDataCollectionEvent(OptimizationTarget segment_id,
                                       TrainingDataCollectionEvent event);

}  // namespace segmentation_platform::stats

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_STATS_H_
