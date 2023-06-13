// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/training_data_collector_impl.h"
#include <cstdint>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/config_parser.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/data_collection/training_data_cache.h"
#include "components/segmentation_platform/internal/database/cached_result_provider.h"
#include "components/segmentation_platform/internal/database/config_holder.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/execution/processing/feature_list_query_processor.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/segmentation_ukm_helper.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/local_state_helper.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/trigger.h"

namespace segmentation_platform {
namespace {
using processing::FeatureListQueryProcessor;

// Minimum intervals between collection.
// TODO(qinmin): make this configurable through finch.
static int kMinimumReportingIntervalInHours = 24;

// Given the last report time, calculate the next report time.
base::Time GetNextReportTime(base::Time last_report_time) {
  // The next report time is determined by |kMinimumReportingIntervalInHours|
  // hours after last report.
  return last_report_time + base::Hours(kMinimumReportingIntervalInHours);
}

// Parse outputs into a map of metric hash of the uma output and its index in
// the output list.
std::map<uint64_t, int> ParseUmaOutputs(
    const proto::SegmentationModelMetadata& metadata) {
  std::map<uint64_t, int> hash_index_map;
  if (!metadata.has_training_outputs())
    return hash_index_map;

  const auto& training_outputs = metadata.training_outputs();
  for (int i = 0; i < training_outputs.outputs_size(); ++i) {
    const auto& output = training_outputs.outputs(i);
    if (!output.has_uma_output() || !output.uma_output().has_uma_feature())
      continue;

    hash_index_map[output.uma_output().uma_feature().name_hash()] = i;
  }
  return hash_index_map;
}

// Returns a list of preferred segment info for each segment ID in the list.
std::map<SegmentId, proto::SegmentInfo> GetPreferredSegmentInfo(
    DefaultModelManager::SegmentInfoList&& segment_list) {
  std::map<SegmentId, proto::SegmentInfo> result;
  for (auto& segment_wrapper : segment_list) {
    SegmentId segment = segment_wrapper->segment_info.segment_id();
    auto it = result.find(segment_wrapper->segment_info.segment_id());
    if (it == result.end() ||
        segment_wrapper->segment_source ==
            DefaultModelManager::SegmentSource::DATABASE) {
      result[segment] = std::move(segment_wrapper->segment_info);
    }
  }

  return result;
}

}  // namespace

struct TrainingDataCollectorImpl::TrainingTimings {
  base::Time prediction_time;
  absl::optional<base::TimeDelta> observation_delayed_task;
};

TrainingDataCollectorImpl::TrainingDataCollectorImpl(
    processing::FeatureListQueryProcessor* processor,
    HistogramSignalHandler* histogram_signal_handler,
    UserActionSignalHandler* user_action_signal_handler,
    StorageService* storage_service,
    PrefService* profile_prefs,
    base::Clock* clock,
    CachedResultProvider* cached_result_provider)
    : segment_info_database_(storage_service->segment_info_database()),
      feature_list_query_processor_(processor),
      histogram_signal_handler_(histogram_signal_handler),
      user_action_signal_handler_(user_action_signal_handler),
      signal_storage_config_(storage_service->signal_storage_config()),
      config_holder_(storage_service->config_holder()),
      clock_(clock),
      result_prefs_(std::make_unique<SegmentationResultPrefs>(profile_prefs)),
      cached_result_provider_(cached_result_provider),
      training_cache_(std::make_unique<TrainingDataCache>(
          storage_service->segment_info_database())),
      default_model_manager_(storage_service->default_model_manager()) {}

TrainingDataCollectorImpl::~TrainingDataCollectorImpl() {
  histogram_signal_handler_->RemoveObserver(this);
  user_action_signal_handler_->RemoveObserver(this);
}

void TrainingDataCollectorImpl::OnModelMetadataUpdated() {
  NOTIMPLEMENTED();
}

void TrainingDataCollectorImpl::OnServiceInitialized() {
  base::flat_set<SegmentId> segment_ids = config_holder_->all_segment_ids();
  if (segment_ids.empty()) {
    return;
  }
  default_model_manager_->GetAllSegmentInfoFromBothModels(
      segment_ids, segment_info_database_,
      base::BindOnce(&TrainingDataCollectorImpl::OnGetSegmentsInfoList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TrainingDataCollectorImpl::OnGetSegmentsInfoList(
    DefaultModelManager::SegmentInfoList segments) {
  histogram_signal_handler_->AddObserver(this);
  user_action_signal_handler_->AddObserver(this);
  std::map<SegmentId, proto::SegmentInfo> segment_list =
      GetPreferredSegmentInfo(std::move(segments));

  for (const auto& segment : segment_list) {
    const proto::SegmentInfo& segment_info = segment.second;

    // Skip the segment if it is not in allowed list.
    if (!SegmentationUkmHelper::GetInstance()->CanUploadTensors(segment_info)) {
      continue;
    }

    // Validate segment info.
    auto validation_result = metadata_utils::ValidateSegmentInfo(segment_info);
    if (validation_result !=
        metadata_utils::ValidationResult::kValidationSuccess) {
      VLOG(1) << "Segment info validation failed for optimization target: "
              << segment.first
              << ", validation result:" << static_cast<int>(validation_result);
      RecordTrainingDataCollectionEvent(
          segment.first,
          stats::TrainingDataCollectionEvent::kMetadataValidationFailed);
      continue;
    }

    const auto& training_config =
        segment_info.model_metadata().training_outputs().trigger_config();

    // Do not upload periodic metrics for exact prediction time config, the
    // trigger is fired by the selector or pref writer when result is changed.
    if (!segment_info.model_metadata()
             .training_outputs()
             .trigger_config()
             .use_exact_prediction_time()) {
      auto hash_index_map = ParseUmaOutputs(segment_info.model_metadata());
      for (const auto& hash_index : hash_index_map) {
        const auto& output =
            segment_info.model_metadata().training_outputs().outputs(
                hash_index.second);
        all_segments_for_training_.insert(segment.first);
        // If tensor length is 0, the output is for immediate collection.
        if (output.uma_output().uma_feature().tensor_length() != 0) {
          continuous_collection_segments_.insert(segment.first);
          continue;
        }
      }
    }

    // Check for unfinished partial training data.
    if (segment_info.training_data_size() > 0) {
      int64_t current_time =
          clock_->Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
      for (int i = 0; i < segment_info.training_data_size(); ++i) {
        const auto& training_data = segment_info.training_data(i);
        if (current_time > training_data.observation_trigger_timestamp()) {
          // Observation is reached for the current training data.
          OnObservationTrigger(
              absl::nullopt,
              TrainingRequestId::FromUnsafeValue(training_data.request_id()),
              segment_info, base::DoNothing());
        }
      }
    }

    // Cache the histograms as outputs of training data, which needs to be
    // immediately reported when the histogram is recorded.
    for (int i = 0; i < training_config.observation_trigger_size(); i++) {
      all_segments_for_training_.insert(segment.first);
      const auto& trigger = training_config.observation_trigger(i);
      if (trigger.has_uma_trigger() &&
          trigger.uma_trigger().has_uma_feature()) {
        const auto& feature = trigger.uma_trigger().uma_feature();
        if (feature.type() == proto::SignalType::USER_ACTION) {
          immediate_trigger_user_actions_[feature.name_hash()].emplace(
              segment.first);
        } else if (feature.type() == proto::SignalType::HISTOGRAM_VALUE ||
                   feature.type() == proto::SignalType::HISTOGRAM_ENUM) {
          std::vector<int> enum_ids;
          for (int j = 0; j < feature.enum_ids_size(); j++) {
            enum_ids.emplace_back(feature.enum_ids(j));
          }
          immediate_trigger_histograms_[feature.name_hash()].emplace(
              std::make_pair(segment.first, enum_ids));
        }
      }
    }
  }

  ReportCollectedContinuousTrainingData();
  // TODO(haileywang): Upload metrics at startup for any training request from
  // previous days, that finished observation.
}

void TrainingDataCollectorImpl::OnHistogramSignalUpdated(
    const std::string& histogram_name,
    base::HistogramBase::Sample sample) {
  // Report training data for all models which output collection is triggered by
  // |histogram_name|.
  auto hash = base::HashMetricName(histogram_name);
  auto it = immediate_trigger_histograms_.find(hash);
  if (it != immediate_trigger_histograms_.end()) {
    auto segments = it->second;
    auto param = absl::make_optional<ImmediateCollectionParam>();
    param->output_metric_hash = hash;
    param->output_value = static_cast<float>(sample);
    for (auto segment : segments) {
      auto segment_id = segment.first;
      auto accepted_enum_ids = segment.second;

      // Process both enum histograms with their corresponding accepted enum ids
      // and value histograms with no enum ids.
      if (accepted_enum_ids.empty() ||
          base::Contains(accepted_enum_ids, sample)) {
        // TODO (ritikagup@) : Add handling for default models, if required.
        segment_info_database_->GetSegmentInfo(
            segment_id, proto::ModelSource::SERVER_MODEL_SOURCE,
            base::BindOnce(
                &TrainingDataCollectorImpl::OnUmaUpdatedReportForSegmentInfo,
                weak_ptr_factory_.GetWeakPtr(), param));
      }
    }
  }
}

void TrainingDataCollectorImpl::OnUserAction(const std::string& user_action,
                                             base::TimeTicks action_time) {
  // Report training data for all models which output collection is triggered by
  // |user_action|.
  auto hash = base::HashMetricName(user_action);
  auto it = immediate_trigger_user_actions_.find(hash);
  if (it != immediate_trigger_user_actions_.end()) {
    auto segments = it->second;
    for (auto segment : segments) {
      // TODO (ritikagup@) : Add handling for default models, if required.
      segment_info_database_->GetSegmentInfo(
          segment, ModelSource::SERVER_MODEL_SOURCE,
          base::BindOnce(
              &TrainingDataCollectorImpl::OnUmaUpdatedReportForSegmentInfo,
              weak_ptr_factory_.GetWeakPtr(), absl::nullopt));
    }
  }
}

void TrainingDataCollectorImpl::OnUmaUpdatedReportForSegmentInfo(
    const absl::optional<ImmediateCollectionParam>& param,
    absl::optional<proto::SegmentInfo> segment) {
  if (segment.has_value()) {
    absl::optional<TrainingRequestId> request_id =
        training_cache_->GetRequestId(segment.value().segment_id());
    if (request_id.has_value()) {
      RecordTrainingDataCollectionEvent(
          segment.value().segment_id(),
          stats::TrainingDataCollectionEvent::kHistogramTriggerHit);
      OnObservationTrigger(param, request_id.value(), segment.value(),
                           base::DoNothing());
    }
  }
}

bool TrainingDataCollectorImpl::CanReportTrainingData(
    const proto::SegmentInfo& segment_info,
    bool include_output) {
  if (!segment_info.has_model_version()) {
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kModelInfoMissing);
    return false;
  }

  const proto::SegmentationModelMetadata& model_metadata =
      segment_info.model_metadata();

  // If UKM is allowed recently, don't upload the metrics.
  DCHECK_LE(model_metadata.min_signal_collection_length(),
            model_metadata.signal_storage_length());
  base::TimeDelta signal_storage_length =
      model_metadata.signal_storage_length() *
      metadata_utils::GetTimeUnit(model_metadata);
  if (!SegmentationUkmHelper::AllowedToUploadData(signal_storage_length,
                                                  clock_)) {
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kPartialDataNotAllowed);
    return false;
  }

  // TODO(ssid): The default models do not set the model update time. Fix this
  // once default model info is stored in database.
  if (segment_info.has_model_update_time_s()) {
    base::TimeDelta min_signal_collection_length =
        model_metadata.min_signal_collection_length() *
        metadata_utils::GetTimeUnit(model_metadata);
    base::Time model_update_time = base::Time::FromDeltaSinceWindowsEpoch(
        base::Seconds(segment_info.model_update_time_s()));

    // Data must be collected for enough time after a new model is downloaded.
    // It's recommended to get the A/B testing experiment fully ramped up before
    // deploying a new model. Or the data collected might be partially based on
    // old behavior of Chrome.
    if (model_update_time + min_signal_collection_length >= clock_->Now()) {
      RecordTrainingDataCollectionEvent(
          segment_info.segment_id(),
          stats::TrainingDataCollectionEvent::kNotEnoughCollectionTime);
      return false;
    }
  }

  // Each input must be collected for enough time.
  if (!signal_storage_config_->MeetsSignalCollectionRequirement(
          model_metadata, include_output)) {
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kNotEnoughCollectionTime);
    return false;
  }

  return true;
}

void TrainingDataCollectorImpl::OnGetTrainingTensors(
    const absl::optional<ImmediateCollectionParam>& param,
    const proto::SegmentInfo& segment_info,
    bool has_error,
    const ModelProvider::Request& input_tensors,
    const ModelProvider::Response& output_tensors) {
  if (has_error) {
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kGetInputTensorsFailed);
    return;
  }

  // TODO(qinmin): update SegmentationUkmHelper::RecordTrainingData()
  // and ukm file for description of the prediction result as it is
  // the segment selection result, rather than model result.
  const Config* config =
      config_holder_->GetConfigForSegmentId(segment_info.segment_id());
  std::string segmentation_key = config ? config->segmentation_key : "";

  std::vector<int> output_indexes;
  auto output_values = output_tensors;
  for (size_t i = 0; i < output_tensors.size(); ++i) {
    output_indexes.emplace_back(i);
  }

  // TODO(haileywang): Find the right output index from the metadata using the
  // matching hash value, in case the client has 2 different histogram triggers
  // in the metadata, the server cannot identify which one was triggered.
  if (param.has_value()) {
    output_indexes.emplace_back(output_tensors.size());
    output_values.emplace_back(param->output_value);
  }

  // Cached results are stored in two formats depending on whether the model is
  // using the legacy output config or the new multi-output model config.
  // |prediction_results| represents the new format which may contains multiple
  // outputs as floats.
  absl::optional<proto::PredictionResult> prediction_result;
  // |selected_segment| represents the legacy format which contains a single
  // segment ID.
  absl::optional<SelectedSegment> selected_segment;

  if (metadata_utils::ConfigUsesLegacyOutput(config)) {
    selected_segment =
        result_prefs_->ReadSegmentationResultFromPref(segmentation_key);
  } else {
    prediction_result =
        cached_result_provider_->GetPredictionResultForClient(segmentation_key);
  }

  auto ukm_source_id = SegmentationUkmHelper::GetInstance()->RecordTrainingData(
      segment_info.segment_id(), segment_info.model_version(), input_tensors,
      output_values, output_indexes, prediction_result, selected_segment);
  if (ukm_source_id == ukm::kInvalidSourceId) {
    VLOG(1) << "Failed to collect training data for segment:"
            << segment_info.segment_id();
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kUkmReportingFailed);
    return;
  }

  RecordTrainingDataCollectionEvent(
      segment_info.segment_id(),
      param.has_value()
          ? stats::TrainingDataCollectionEvent::kImmediateCollectionSuccess
          : stats::TrainingDataCollectionEvent::kContinousCollectionSuccess);
  if (!param.has_value()) {
    LocalStateHelper::GetInstance().SetPrefTime(
        kSegmentationLastCollectionTimePref, clock_->Now());
  }
}

void TrainingDataCollectorImpl::ReportCollectedContinuousTrainingData() {
  if (continuous_collection_segments_.empty())
    return;

  base::Time last_collection_time = LocalStateHelper::GetInstance().GetPrefTime(
      kSegmentationLastCollectionTimePref);
  base::Time next_collection_time = GetNextReportTime(last_collection_time);
  if (clock_->Now() >= next_collection_time) {
    for (auto id : continuous_collection_segments_) {
      OnDecisionTime(id, /*input_context=*/nullptr,
                     proto::TrainingOutputs::TriggerConfig::PERIODIC);
    }
  }
}

void TrainingDataCollectorImpl::CollectTrainingData(
    SegmentId segment_id,
    TrainingRequestId request_id,
    const TrainingLabels& param,
    SuccessCallback callback) {
  // TODO (ritikagup@) : Add handling for default models, if required.
  auto segment_info = segment_info_database_->GetCachedSegmentInfo(
      segment_id, proto::ModelSource::SERVER_MODEL_SOURCE);
  if (!segment_info) {
    return;
  }
  absl::optional<TrainingDataCollector::ImmediateCollectionParam>
      immediate_param;
  if (param.output_metric) {
    immediate_param = TrainingDataCollector::ImmediateCollectionParam();
    immediate_param->output_metric_hash =
        base::HashMetricName(param.output_metric.value().first);
    immediate_param->output_value =
        static_cast<float>(param.output_metric.value().second);
  }
  OnObservationTrigger(immediate_param, request_id, segment_info.value(),
                       std::move(callback));
}

TrainingRequestId TrainingDataCollectorImpl::OnDecisionTime(
    proto::SegmentId id,
    scoped_refptr<InputContext> input_context,
    DecisionType type) {
  if (all_segments_for_training_.count(id) == 0) {
    return TrainingRequestId();
  }

  const TrainingRequestId request_id = training_cache_->GenerateNextId();

  default_model_manager_->GetAllSegmentInfoFromBothModels(
      {id}, segment_info_database_,
      base::BindOnce(&TrainingDataCollectorImpl::OnGetSegmentInfoAtDecisionTime,
                     weak_ptr_factory_.GetWeakPtr(), id, request_id, type,
                     input_context));

  return request_id;
}

void TrainingDataCollectorImpl::OnGetSegmentInfoAtDecisionTime(
    proto::SegmentId segment_id,
    TrainingRequestId request_id,
    DecisionType type,
    scoped_refptr<InputContext> input_context,
    DefaultModelManager::SegmentInfoList segment_list) {
  auto preferred_segment_info =
      GetPreferredSegmentInfo(std::move(segment_list));
  auto it = preferred_segment_info.find(segment_id);

  // If no segment info list has been found.
  if (it == preferred_segment_info.end()) {
    RecordTrainingDataCollectionEvent(
        segment_id, stats::TrainingDataCollectionEvent::kNoSegmentInfo);
    return;
  }

  const proto::SegmentInfo& segment_info = it->second;

  if (!CanReportTrainingData(segment_info, /*include_outputs*/ false)) {
    RecordTrainingDataCollectionEvent(
        segment_id,
        stats::TrainingDataCollectionEvent::kDisallowedForRecording);
    return;
  }

  TrainingTimings training_request = ComputeDecisionTiming(segment_info);

  auto is_periodic = (type != proto::TrainingOutputs::TriggerConfig::ONDEMAND);
  RecordTrainingDataCollectionEvent(
      segment_info.segment_id(),
      is_periodic
          ? stats::TrainingDataCollectionEvent::kContinousCollectionStart
          : stats::TrainingDataCollectionEvent::kImmediateCollectionStart);

  // Start training data collection and generate training data inputs.
  base::Time unused;
  feature_list_query_processor_->ProcessFeatureList(
      segment_info.model_metadata(), input_context, segment_id,
      /*prediction_time*/ training_request.prediction_time,
      /*observation_time*/ unused,
      /*process_option=*/FeatureListQueryProcessor::ProcessOption::kInputsOnly,
      base::BindOnce(
          &TrainingDataCollectorImpl::OnGetTrainingTensorsAtDecisionTime,
          weak_ptr_factory_.GetWeakPtr(), request_id, training_request,
          segment_info));
}

void TrainingDataCollectorImpl::OnGetTrainingTensorsAtDecisionTime(
    TrainingRequestId request_id,
    const TrainingTimings& training_request,
    const proto::SegmentInfo& segment_info,
    bool has_error,
    const ModelProvider::Request& input_tensors,
    const ModelProvider::Response& output_tensors) {
  // Store inputs to cache.
  proto::TrainingData training_data;
  bool store_to_disk = FillTrainingData(
      request_id, training_request, input_tensors, segment_info, training_data);

  if (store_to_disk) {
    // Calculate observation time only if training data needs to be stored to
    // disk.
    int64_t observation_timestamp =
        (training_request.prediction_time +
         training_request.observation_delayed_task.value())
            .ToDeltaSinceWindowsEpoch()
            .InMicroseconds();
    training_data.set_observation_trigger_timestamp(observation_timestamp);
  }

  training_cache_->StoreInputs(segment_info.segment_id(),
                               std::move(training_data),
                               /*save_to_db=*/store_to_disk);

  if (has_error) {
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kGetInputTensorsFailed);
  } else {
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kCollectAndStoreInputsSuccess);
  }

  // Set up delayed output recordings based on time delay triggers defined
  // in model metadata.
  // TODO(haileywang): This is slightly inaccurate since the the delay timer is
  // only started after the input training tensors are cached.
  if (training_request.observation_delayed_task) {
    if (training_request.observation_delayed_task.value().is_zero()) {
      RecordTrainingDataCollectionEvent(
          segment_info.segment_id(),
          stats::TrainingDataCollectionEvent::kImmediateObservationPosted);
    } else {
      RecordTrainingDataCollectionEvent(
          segment_info.segment_id(),
          stats::TrainingDataCollectionEvent::kDelayedTaskPosted);
    }

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TrainingDataCollectorImpl::OnObservationTrigger,
                       weak_ptr_factory_.GetWeakPtr(), absl::nullopt,
                       request_id, segment_info, base::DoNothing()),
        *training_request.observation_delayed_task);
  } else {
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kWaitingForNonDelayedTrigger);
  }
}

void TrainingDataCollectorImpl::OnObservationTrigger(
    const absl::optional<ImmediateCollectionParam>& param,
    TrainingRequestId request_id,
    const proto::SegmentInfo& segment_info,
    SuccessCallback callback) {
  if (request_id.is_null()) {
    return;
  }

  RecordTrainingDataCollectionEvent(
      segment_info.segment_id(),
      stats::TrainingDataCollectionEvent::kObservationTimeReached);

  if (!CanReportTrainingData(segment_info, /*include_outputs*/ true)) {
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kObservationDisallowed);
    return;
  }

  // Retrieve input tensor from cache.
  training_cache_->GetInputsAndDelete(
      segment_info.segment_id(), request_id,
      base::BindOnce(&TrainingDataCollectorImpl::OnGetStoredTrainingData,
                     weak_ptr_factory_.GetWeakPtr(), param, segment_info,
                     std::move(callback)));
}

void TrainingDataCollectorImpl::OnGetStoredTrainingData(
    const absl::optional<ImmediateCollectionParam>& param,
    const proto::SegmentInfo& segment_info,
    SuccessCallback callback,
    absl::optional<proto::TrainingData> input) {
  if (!input.has_value()) {
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kTrainingDataMissing);
    std::move(callback).Run(/*success*/ false);
    return;
  }

  // Observation trigger always gets prediction time from cached partial
  // tensor.
  base::Time prediction_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(input->decision_timestamp()));
  base::Time observation_time =
      ComputeObservationTiming(segment_info, prediction_time);

  // Generate training data output.
  feature_list_query_processor_->ProcessFeatureList(
      segment_info.model_metadata(), /*input_context=*/nullptr,
      segment_info.segment_id(), prediction_time, observation_time,
      /*process_option=*/FeatureListQueryProcessor::ProcessOption::kOutputsOnly,
      base::BindOnce(
          &TrainingDataCollectorImpl::OnGetOutputsOnObservationTrigger,
          weak_ptr_factory_.GetWeakPtr(), param, segment_info,
          ModelProvider::Response(input.value().inputs().begin(),
                                  input.value().inputs().end())));

  std::move(callback).Run(/*success*/ true);
}

void TrainingDataCollectorImpl::OnGetOutputsOnObservationTrigger(
    const absl::optional<ImmediateCollectionParam>& param,
    const proto::SegmentInfo& segment_info,
    const ModelProvider::Request& cached_input_tensors,
    bool has_error,
    const ModelProvider::Request& input_tensors,
    const ModelProvider::Response& output_tensors) {
  // Upload input and output tensors.
  // TODO(haileywang): Add state in cache for each request; never seen,
  // fulfilled, unfullfilled. (Or make triggers cancellable callbacks)
  // TODO(haileywang): Add output processing failure uma histogram (maybe
  // success histogram too).
  OnGetTrainingTensors(param, segment_info, has_error, cached_input_tensors,
                       output_tensors);
}

TrainingDataCollectorImpl::TrainingTimings
TrainingDataCollectorImpl::ComputeDecisionTiming(
    const proto::SegmentInfo& info) const {
  TrainingDataCollectorImpl::TrainingTimings training_request;
  const auto& training_config =
      info.model_metadata().training_outputs().trigger_config();
  auto type = training_config.decision_type();
  base::Time current_time = clock_->Now();

  // Default for unset decision type is periodic type.
  bool is_periodic = (type != proto::TrainingOutputs::TriggerConfig::ONDEMAND);

  // Check for delay triggers in the config.
  absl::optional<uint64_t> delay_sec;
  for (int i = 0; i < training_config.observation_trigger_size(); i++) {
    const auto& trigger = training_config.observation_trigger(i);
    if (trigger.has_delay_sec()) {
      delay_sec = trigger.delay_sec();
    }
  }

  bool exact_prediction_time = training_config.use_exact_prediction_time();

  if (is_periodic) {
    if (delay_sec && exact_prediction_time) {
      // Triggered by the client pref update, so use current time.
      // TODO(ssid): This is not accurate since the client did not start using
      // the new result yet, add the client usage timestamp support in prefs
      // to get better prediction timestamp.
      training_request.prediction_time = current_time;
      training_request.observation_delayed_task = base::Seconds(*delay_sec);
    } else if (delay_sec) {
      // We are allowed to use any point in the past as prediction time. So, go
      // back delay time period for collection period.
      training_request.prediction_time =
          current_time - base::Seconds(*delay_sec);
      training_request.observation_delayed_task = base::TimeDelta();
    } else {
      training_request.prediction_time = current_time;
      // Post trigger immediately.
      training_request.observation_delayed_task = base::TimeDelta();
    }
  } else {
    // For on demand cases and periodic cases with no delay.
    training_request.prediction_time = current_time;
    if (delay_sec) {
      training_request.observation_delayed_task = base::Seconds(*delay_sec);
    } else {
      // If on demand and delay is not provided then wait for histogram or
      // client trigger instead.
      training_request.observation_delayed_task = absl::nullopt;
    }
  }

  return training_request;
}

base::Time TrainingDataCollectorImpl::ComputeObservationTiming(
    const proto::SegmentInfo& info,
    base::Time prediction_time) const {
  const auto& training_config =
      info.model_metadata().training_outputs().trigger_config();
  base::Time current_time = clock_->Now();
  bool is_periodic = (training_config.decision_type() !=
                      proto::TrainingOutputs::TriggerConfig::ONDEMAND);
  bool flexible_observation_period =
      training_config.use_flexible_observation_time();

  // Check for delay triggers in the config.
  absl::optional<uint64_t> delay_sec;
  for (int i = 0; i < training_config.observation_trigger_size(); i++) {
    const auto& trigger = training_config.observation_trigger(i);
    if (trigger.has_delay_sec()) {
      delay_sec = trigger.delay_sec();
    }
  }

  base::Time observation_time = current_time;

  if (delay_sec && !flexible_observation_period) {
    // If exact observation period is needed, use the time right after
    // prediction.
    observation_time = prediction_time + base::Seconds(*delay_sec);
  }
  if (is_periodic && !delay_sec) {
    // If delay is not set, then observation should be reset, so the feature
    // processor uses prediction time as observation time.
    observation_time = base::Time();
  }

  return observation_time;
}

bool TrainingDataCollectorImpl::FillTrainingData(
    TrainingRequestId request_id,
    const TrainingTimings& training_request,
    const ModelProvider::Request& input_tensors,
    const proto::SegmentInfo& segment_info,
    proto::TrainingData& training_data) {
  for (const auto& input : input_tensors) {
    training_data.add_inputs(input);
  }
  training_data.set_decision_timestamp(
      training_request.prediction_time.ToDeltaSinceWindowsEpoch()
          .InMicroseconds());
  training_data.set_request_id(request_id.GetUnsafeValue());

  const auto& training_config =
      segment_info.model_metadata().training_outputs().trigger_config();
  bool is_periodic = (training_config.decision_type() !=
                      proto::TrainingOutputs::TriggerConfig::ONDEMAND);

  // Only periodic segments need storage to disk and can be multi-session.
  // If the exact prediction time is not used, we could recompute inputs at
  // observation time so we don't need to store to disk.
  // If the observation is triggered immediately, we don't need to store to
  // disk.
  // If delay is not specified, then the collector cannot know when to trigger
  // observation and the training data will live in database forever. So, it is
  // safe to verify the delay before storing to disk.
  bool store_to_disk = is_periodic &&
                       training_config.use_exact_prediction_time() &&
                       training_request.observation_delayed_task.has_value();

  return store_to_disk;
}

}  // namespace segmentation_platform
