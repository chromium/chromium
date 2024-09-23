// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_STATS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_STATS_H_

#include <list>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/selection/segment_result_provider.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "components/segmentation_platform/public/segment_selection_result.h"

namespace segmentation_platform::stats {

using proto::SegmentId;

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

// Records the time difference between when a new version of model from
// optimization guide is available and when the model is initialized in the
// client.
void RecordModelUpdateTimeDifference(SegmentId segment_id,
                                     int64_t model_update_time);

// Records the result of segment selection whenever segment selection is
// computed.
void RecordSegmentSelectionComputed(
    const Config& config,
    SegmentId new_selection,
    std::optional<SegmentId> previous_selection);

// Records the post processed result whenever computed. This is recorded when
// results are obtained by eithier executing the model or getting a valid score
// from database.
void RecordClassificationResultComputed(
    const Config& config,
    const proto::PredictionResult& new_result);

// Records from which old value to which new value the topmost label is changing
// to when prefs expired and is updated with new result.
void RecordClassificationResultUpdated(
    const Config& config,
    const proto::PredictionResult* old_result,
    const proto::PredictionResult& new_result);

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
// Records whether any incoming ML had metadata attached that
// we were able to parse.
void RecordModelDeliveryHasMetadata(SegmentId segment_id, bool has_metadata);
// Records the number of tensor features an updated server or embedded model
// has.
void RecordModelDeliveryMetadataFeatureCount(SegmentId segment_id,
                                             proto::ModelSource model_source,
                                             size_t count);
// Records the result of validating the metadata of an incoming server or
// embedded model. Recorded before and after it has been merged with the already
// stored metadata.
void RecordModelDeliveryMetadataValidation(
    SegmentId segment_id,
    proto::ModelSource model_source,
    bool processed,
    metadata_utils::ValidationResult validation_result);
// Record what type of server or embedded model metadata we received .
void RecordModelDeliveryReceived(SegmentId segment_id,
                                 proto::ModelSource model_source);
// Records the result of attempting to save an updated version of the server or
// embedded model metadata.
void RecordModelDeliverySaveResult(SegmentId segment_id,
                                   proto::ModelSource model_source,
                                   bool success);
// Records the result of attempting to delete the previous version of a server
// model metadata.
void RecordModelDeliveryDeleteResult(SegmentId segment_id,
                                     proto::ModelSource model_source,
                                     bool success);
// Records whether the currently stored segment_id matches the incoming
// segment_id for a particular model_source, as these are expected to match.
void RecordModelDeliverySegmentIdMatches(SegmentId segment_id,
                                         proto::ModelSource model_source,
                                         bool matches);

// Model Execution metrics.
// Records the duration of processing a single ML feature. This only takes into
// account the time it takes to process (aggregate) a feature result, not
// fetching it from the database. It also takes into account filtering any
// enum histograms.
void RecordModelExecutionDurationFeatureProcessing(SegmentId segment_id,
                                                   base::TimeDelta duration);
// Records the duration of executing an ML model. This only takes into account
// the time it takes to invoke and wait for a result from the underlying ML
// infrastructure from //components/optimization_guide, and not fetching the
// relevant data from the database.
void RecordModelExecutionDurationModel(SegmentId segment_id,
                                       bool success,
                                       base::TimeDelta duration);
// Records the duration of fetching data for, processing, and executing an ML
// model.
void RecordModelExecutionDurationTotal(SegmentId segment_id,
                                       ModelExecutionStatus status,
                                       base::TimeDelta duration);

// Records the total duration for GetClassificationResult API starting from the
// time request arrives in segmentation service until the result has been
// returned. It includes feature processing and model execution as well.
void RecordClassificationRequestTotalDuration(const Config& config,
                                              base::TimeDelta duration);

// Records the total duration of on-demand segment selection which includes
// running all the models associated with the client and computing result.
void RecordOnDemandSegmentSelectionDuration(
    const Config& config,
    const SegmentSelectionResult& result,
    base::TimeDelta duration);
// Records the result value after successfully executing an ML model.
void RecordModelExecutionResult(
    SegmentId segment_id,
    float result,
    proto::SegmentationModelMetadata::OutputDescription return_type);
// Records the raw model score. Records each element of the result tensor as a
// separate histogram identified by its index. For binary and multi-class
// models, the raw score is multiplied by 100 to transform as percent score,
// whereas for binned classifier the result is recorded as is in a percent
// range.
void RecordModelExecutionResult(SegmentId segment_id,
                                const ModelProvider::Response& result,
                                proto::OutputConfig output_config);
// Records whether the result value of of executing an ML model was successfully
// saved.
void RecordModelExecutionSaveResult(SegmentId segment_id, bool success);
// Records the final execution status for any ML model execution.
void RecordModelExecutionStatus(SegmentId segment_id,
                                bool default_provider,
                                ModelExecutionStatus status);
// Records the percent of features in a tensor that are equal to 0 when the
// segmentation model is executed.
void RecordModelExecutionZeroValuePercent(SegmentId segment_id,
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

// Records the result of persisting SegmentInfo changes to disk.
void RecordSegmentInfoDatabaseUpdateEntriesResult(SegmentId segment_id,
                                                  bool success);

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
  kDeprecatedPlatformDisabled = 0,
  kSelectionAvailableInPrefs = 1,
  kServerModelDatabaseScoreNotReady = 2,
  kServerModelSignalsNotCollected = 3,
  kSelectionTtlNotExpired = 4,
  kAtLeastOneModelFailedExecution = 5,
  kAtLeastOneModelNeedsMoreSignals = 6,
  kAtLeastOneModelWithInvalidMetadata = 7,
  kFailedToSaveModelResult = 8,
  kInvalidSelectionResultInPrefs = 9,
  kDBInitFailure = 10,
  kServerModelSegmentInfoNotAvailable = 11,
  kDefaultModelSignalsNotCollected = 12,
  kDefaultModelExecutionFailed = 13,
  kDefaultModelSegmentInfoNotAvailable = 14,
  kServerModelExecutionFailed = 15,
  kSelectionAvailableInProtoPrefs = 16,
  kInvalidSelectionResultInProtoPrefs = 17,
  kProtoPrefsUpdateNotRequired = 18,
  kProtoPrefsUpdated = 19,
  kServerModelDatabaseScoreUsed = 20,
  kDefaultModelExecutionScoreUsed = 21,
  kServerModelExecutionScoreUsed = 22,
  kMultiOutputNotSupported = 23,
  kOnDemandModelExecutionFailed = 24,
  kClassificationResultFromPrefs = 25,
  kClassificationResultNotAvailableInPrefs = 26,
  kDefaultModelDatabaseScoreUsed = 27,
  kDefaultModelDatabaseScoreNotReady = 28,
  kCachedResultUnavailableExecutingOndemand = 29,
  kOnDemandExecutionFailedReturningCachedResult = 30,
  kMaxValue = kOnDemandExecutionFailedReturningCachedResult,
};

// Records the reason for failure or success to compute a segment selection.
void RecordSegmentSelectionFailure(const Config& config,
                                   SegmentationSelectionFailureReason reason);

// Keep in sync with SegmentationPlatformFeatureProcessingError in
// //tools/metrics/histograms/enums.xml.
enum class FeatureProcessingError {
  kUkmEngineDisabled = 0,
  kUmaValidationError = 1,
  kSqlValidationError = 2,
  kCustomInputError = 3,
  kSqlBindValuesError = 4,
  kSqlQueryRunError = 5,
  kResultTensorError = 6,
  kSuccess = 7,
  kMaxValue = kSuccess,
};

// Return a string display for the given FeatureProcessingError.
std::string FeatureProcessingErrorToString(FeatureProcessingError error);

// Records the type of error encountered during feature processing.
void RecordFeatureProcessingError(SegmentId segment_id,
                                  FeatureProcessingError error);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "SegmentationModelAvailability" in //tools/metrics/histograms/enums.xml.
enum class SegmentationModelAvailability {
  kModelHandlerCreated = 0,
  kModelAvailable = 1,
  kMetadataInvalid = 2,
  kNoModelAvailable = 3,
  kMaxValue = kNoModelAvailable
};
// Records the availability of segmentation models for each target needed.
void RecordModelAvailability(SegmentId segment_id,
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
  kPartialDataNotAllowed = 7,
  kContinousCollectionStart = 8,
  kContinousCollectionSuccess = 9,
  kCollectAndStoreInputsSuccess = 10,
  kObservationTimeReached = 11,
  kDelayedTaskPosted = 12,
  kImmediateObservationPosted = 13,
  kWaitingForNonDelayedTrigger = 14,
  kHistogramTriggerHit = 15,
  kNoSegmentInfo = 16,
  kDisallowedForRecording = 17,
  kObservationDisallowed = 18,
  kTrainingDataMissing = 19,
  kOnDecisionTimeTypeMistmatch = 20,
  kDelayTriggerSampled = 21,
  kContinousExactPredictionTimeCollectionStart = 22,
  kContinousExactPredictionTimeCollectionSuccess = 23,
  kMaxValue = kContinousExactPredictionTimeCollectionSuccess
};

std::string TrainingDataCollectionEventToErrorMsg(
    TrainingDataCollectionEvent event);

// Records analytics for training data collection.
void RecordTrainingDataCollectionEvent(SegmentId segment_id,
                                       TrainingDataCollectionEvent event);

SegmentationSelectionFailureReason GetSuccessOrFailureReason(
    SegmentResultProvider::ResultState result_state);

// Helper to collect UMA metrics and record in a non-blocking thread.
class BackgroundUmaRecorder {
 public:
  // Delay to collect metrics and record. Set to short time so if Chrome dies we
  // don't lose too many metrics.
  constexpr static base::TimeDelta kMetricsCollectionDelay = base::Seconds(5);

  static BackgroundUmaRecorder& GetInstance();

  BackgroundUmaRecorder(const BackgroundUmaRecorder&) = delete;
  BackgroundUmaRecorder& operator=(const BackgroundUmaRecorder&) = delete;

  // If initialized then records metrics in a worker thread, otherwise records
  // metrics in current thread, blocking. Can be called multiple times safely.
  void Initialize();
  void InitializeForTesting(
      scoped_refptr<base::SequencedTaskRunner> bg_task_runner);

  // Force flush all samples in current thread.
  void FlushSamples();

  // Add metrics to UMA.
  void AddMetric(base::OnceClosure add_sample);

  scoped_refptr<base::SequencedTaskRunner> bg_task_runner_for_testing() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_check_);
    return bg_task_runner_;
  }

 private:
  friend class base::NoDestructor<BackgroundUmaRecorder>;

  BackgroundUmaRecorder();
  ~BackgroundUmaRecorder();

  base::Lock lock_;
  std::list<base::OnceClosure> add_samples_ GUARDED_BY(lock_);
  bool pending_task_ GUARDED_BY(lock_){false};

  scoped_refptr<base::SequencedTaskRunner> bg_task_runner_;
  // Protects `bg_task_runner_`. If we need to record metrics from non-main
  // thread, do not use this class and record directly.
  SEQUENCE_CHECKER(sequence_check_);
  base::WeakPtrFactory<BackgroundUmaRecorder> weak_factory_{this};
};

}  // namespace segmentation_platform::stats

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_STATS_H_
