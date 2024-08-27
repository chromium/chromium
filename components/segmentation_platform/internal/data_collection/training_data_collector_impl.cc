// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/training_data_collector_impl.h"

#include <cstdint>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/rand_util.h"
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
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/segmentation_ukm_helper.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/local_state_helper.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/trigger.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

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

// Returns a list of preferred segment info for each segment ID in the list.
std::map<SegmentId, const proto::SegmentInfo*> GetPreferredSegmentInfo(
    std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segment_list) {
  std::map<SegmentId, const proto::SegmentInfo*> result;
  for (auto& segment_id_and_info : *segment_list) {
    SegmentId segment_id = segment_id_and_info.first;
    auto it = result.find(segment_id);
    if (it == result.end() || segment_id_and_info.second->model_source() !=
                                  proto::ModelSource::DEFAULT_MODEL_SOURCE) {
      result[segment_id] = segment_id_and_info.second;
    }
  }
  return result;
}

bool IsPeriodic(const proto::SegmentInfo& info) {
  DecisionType type =
      info.model_metadata().training_outputs().trigger_config().decision_type();

  // Add exception allowlist for old models that did not have model type set.
  // This is needed because some legacy target models do not have the
  // decision_type field set. For those models, a periodic type should be
  // defaulted.
  if (info.segment_id() ==
          proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB ||
      info.segment_id() ==
          proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE ||
      info.segment_id() ==
          proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE) {
    return type != proto::TrainingOutputs::TriggerConfig::ONDEMAND;
  }

  return type == proto::TrainingOutputs::TriggerConfig::PERIODIC;
}

bool NeedsExactPredictionTime(const proto::SegmentInfo& segment_info) {
  return segment_info.model_metadata()
      .training_outputs()
      .trigger_config()
      .use_exact_prediction_time();
}

constexpr base::FeatureParam<int> TimeDelaySamplingRate{
    &features::kSegmentationPlatformTimeDelaySampling,
    /*name=*/"SamplingRate", /*default_value=*/20};

}  // namespace

struct TrainingDataCollectorImpl::TrainingTimings {
  base::Time prediction_time;
  std::optional<base::TimeDelta> observation_delayed_task;
};

TrainingDataCollectorImpl::TrainingDataCollectorImpl(
    const PlatformOptions& platform_options,
    processing::FeatureListQueryProcessor* processor,
    HistogramSignalHandler* histogram_signal_handler,
    UserActionSignalHandler* user_action_signal_handler,
    StorageService* storage_service,
    PrefService* profile_prefs,
    base::Clock* clock,
    CachedResultProvider* cached_result_provider)
    : platform_options_(platform_options),
      segment_info_database_(storage_service->segment_info_database()),
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
      time_trigger_sampling_rate_(TimeDelaySamplingRate.Get()) {}

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
  auto available_segments =
      segment_info_database_->GetSegmentInfoForBothModels(segment_ids);
  OnGetSegmentsInfoList(std::move(available_segments));
}

void TrainingDataCollectorImpl::OnGetSegmentsInfoList(
    std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segments) {
  histogram_signal_handler_->AddObserver(this);
  user_action_signal_handler_->AddObserver(this);
  std::map<SegmentId, const proto::SegmentInfo*> segment_list =
      GetPreferredSegmentInfo(std::move(segments));

  for (const auto& segment : segment_list) {
    const proto::SegmentInfo& segment_info = *segment.second;

    // Skip the segment if training data is not needed.
    if (!SegmentationUkmHelper::GetInstance()->IsUploadRequested(
            segment_info)) {
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
    if (!NeedsExactPredictionTime(segment_info)) {
      all_segments_for_training_.insert(
          std::make_pair(segment.first, segment_info.model_source()));
      // Add periodic models to continuous collection segments.
      if (IsPeriodic(segment_info)) {
        continuous_collection_segments_.insert(segment.first);
      }
    }

    // Check for unfinished partial training data.
    if (segment_info.training_data_size() > 0) {
      int64_t current_time =
          clock_->Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
      for (int i = 0; i < segment_info.training_data_size(); ++i) {
        const auto& training_data = segment_info.training_data(i);
        if (current_time > training_data.observation_trigger_timestamp()) {
          VLOG(1) << "Periodic observation ended for "
                  << proto::SegmentId_Name(segment_info.segment_id())
                  << " with ModelSource is "
                  << proto::ModelSource_Name(segment_info.model_source());
          // Observation is reached for the current training data.
          PostObservationTask(
              TrainingRequestId::FromUnsafeValue(training_data.request_id()),
              segment_info, base::TimeDelta(),
              stats::TrainingDataCollectionEvent::kDelayedTaskPosted);
        }
      }
    }

    // Cache the histograms as outputs of training data, which needs to be
    // immediately reported when the histogram is recorded.
    for (int i = 0; i < training_config.observation_trigger_size(); i++) {
      all_segments_for_training_.insert(
          std::make_pair(segment.first, segment_info.model_source()));
      const auto& trigger = training_config.observation_trigger(i);
      if (trigger.has_uma_trigger() &&
          trigger.uma_trigger().has_uma_feature()) {
        const auto& feature = trigger.uma_trigger().uma_feature();
        if (feature.type() == proto::SignalType::USER_ACTION) {
          immediate_trigger_user_actions_[feature.name_hash()].emplace(
              std::pair(segment.first, segment_info.model_source()));
        } else if (feature.type() == proto::SignalType::HISTOGRAM_VALUE ||
                   feature.type() == proto::SignalType::HISTOGRAM_ENUM) {
          std::vector<int> enum_ids;
          for (int j = 0; j < feature.enum_ids_size(); j++) {
            enum_ids.emplace_back(feature.enum_ids(j));
          }
          immediate_trigger_histograms_[feature.name_hash()].emplace(
              std::make_pair(
                  std::make_pair(segment.first, segment_info.model_source()),
                  enum_ids));
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
    auto param = std::make_optional<ImmediateCollectionParam>();
    param->output_metric_name = histogram_name;
    param->output_metric_hash = hash;
    param->output_value = static_cast<float>(sample);
    for (auto segment : segments) {
      auto segment_id = segment.first.first;
      auto model_source = segment.first.second;
      auto accepted_enum_ids = segment.second;

      // Process both enum histograms with their corresponding accepted enum ids
      // and value histograms with no enum ids.
      if (accepted_enum_ids.empty() ||
          base::Contains(accepted_enum_ids, sample)) {
        const SegmentInfo* info = segment_info_database_->GetCachedSegmentInfo(
            segment_id, model_source);
        OnUmaUpdatedReportForSegmentInfo(param, info);
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
      const SegmentInfo* info = segment_info_database_->GetCachedSegmentInfo(
          segment.first, segment.second);
      OnUmaUpdatedReportForSegmentInfo(std::nullopt, info);
    }
  }
}

void TrainingDataCollectorImpl::SetSamplingRateForTesting(
    uint64_t sampling_rate) {
  time_trigger_sampling_rate_ = sampling_rate;
}

void TrainingDataCollectorImpl::OnUmaUpdatedReportForSegmentInfo(
    const std::optional<ImmediateCollectionParam>& param,
    const proto::SegmentInfo* segment) {
  if (segment) {
    std::optional<TrainingRequestId> request_id = training_cache_->GetRequestId(
        segment->segment_id(), segment->model_source());
    if (request_id.has_value()) {
      RecordTrainingDataCollectionEvent(
          segment->segment_id(),
          stats::TrainingDataCollectionEvent::kHistogramTriggerHit);
      VLOG(1) << "Observation ended for "
              << proto::SegmentId_Name(segment->segment_id()) << " "
              << (param ? param->output_metric_name : "");

      OnObservationTrigger(param, request_id.value(), *segment,
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

  if (platform_options_.force_refresh_results) {
    return true;
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
    const std::optional<ImmediateCollectionParam>& param,
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

  ukm::SourceId client_ukm_source_id = ukm::kInvalidSourceId;

  // TODO(haileywang): Find the right output index from the metadata using the
  // matching hash value, in case the client has 2 different histogram triggers
  // in the metadata, the server cannot identify which one was triggered.
  if (param.has_value()) {
    output_indexes.emplace_back(output_tensors.size());
    output_values.emplace_back(param->output_value);
    client_ukm_source_id = param->ukm_source_id;
  }

  // Cached results are stored in two formats depending on whether the model is
  // using the legacy output config or the new multi-output model config.
  // |prediction_results| represents the new format which may contains multiple
  // outputs as floats.
  std::optional<proto::PredictionResult> prediction_result;
  // |selected_segment| represents the legacy format which contains a single
  // segment ID.
  std::optional<SelectedSegment> selected_segment;

  if (metadata_utils::ConfigUsesLegacyOutput(config)) {
    selected_segment =
        result_prefs_->ReadSegmentationResultFromPref(segmentation_key);
  } else {
    prediction_result =
        cached_result_provider_->GetPredictionResultForClient(segmentation_key);
  }

  auto ukm_source_id = SegmentationUkmHelper::GetInstance()->RecordTrainingData(
      segment_info.segment_id(), segment_info.model_version(),
      client_ukm_source_id, input_tensors, output_values, output_indexes,
      prediction_result, selected_segment);
  if (ukm_source_id == ukm::kInvalidSourceId) {
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kUkmReportingFailed);
    return;
  }

  auto collection_event =
      stats::TrainingDataCollectionEvent::kImmediateCollectionSuccess;
  if (!param.has_value()) {
    collection_event =
        stats::TrainingDataCollectionEvent::kContinousCollectionSuccess;
    if (NeedsExactPredictionTime(segment_info)) {
      collection_event = collection_event = stats::TrainingDataCollectionEvent::
          kContinousExactPredictionTimeCollectionSuccess;
    }
  }
  RecordTrainingDataCollectionEvent(segment_info.segment_id(),
                                    collection_event);

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
                     proto::TrainingOutputs::TriggerConfig::PERIODIC,
                     std::nullopt);
    }
  }
}

void TrainingDataCollectorImpl::CollectTrainingData(
    SegmentId segment_id,
    TrainingRequestId request_id,
    ukm::SourceId ukm_source_id,
    const TrainingLabels& param,
    SuccessCallback callback) {
  auto available_segments =
      segment_info_database_->GetSegmentInfoForBothModels({segment_id});
  std::map<SegmentId, const proto::SegmentInfo*> preferred_segment_infos =
      GetPreferredSegmentInfo(std::move(available_segments));
  auto it = preferred_segment_infos.find(segment_id);
  // If no segment info list has been found.
  if (it == preferred_segment_infos.end()) {
    return;
  }
  const auto* segment_info = it->second;

  std::optional<TrainingDataCollector::ImmediateCollectionParam>
      immediate_param;
  if (param.output_metric) {
    immediate_param = TrainingDataCollector::ImmediateCollectionParam();
    immediate_param->output_metric_hash =
        base::HashMetricName(param.output_metric.value().first);
    immediate_param->output_value =
        static_cast<float>(param.output_metric.value().second);
    immediate_param->ukm_source_id = ukm_source_id;
  }
  VLOG(1) << "Observation ended for " << proto::SegmentId_Name(segment_id)
          << " " << (param.output_metric ? param.output_metric->first : "");
  OnObservationTrigger(immediate_param, request_id, *segment_info,
                       std::move(callback));
}

TrainingRequestId TrainingDataCollectorImpl::OnDecisionTime(
    proto::SegmentId segment_id,
    scoped_refptr<InputContext> input_context,
    DecisionType type,
    std::optional<ModelProvider::Request> inputs,
    bool decision_result_update_trigger) {
  if (all_segments_for_training_.count(segment_id) == 0) {
    return TrainingRequestId();
  }

  const TrainingRequestId request_id = training_cache_->GenerateNextId();

  auto* segment_info = segment_info_database_->GetCachedSegmentInfo(
      segment_id, all_segments_for_training_[segment_id]);

  // If no segment info has been found.
  if (!segment_info) {
    RecordTrainingDataCollectionEvent(
        segment_id, stats::TrainingDataCollectionEvent::kNoSegmentInfo);
    return request_id;
  }

  // Don't collect training data for periodic collection for exact prediction
  // time if exact prediction time is not set.
  if (type == proto::TrainingOutputs::TriggerConfig::PERIODIC &&
      decision_result_update_trigger &&
      !NeedsExactPredictionTime(*segment_info)) {
    return request_id;
  }

  OnGetSegmentInfoAtDecisionTime(segment_id, request_id, type, input_context,
                                 *segment_info, std::move(inputs));
  return request_id;
}

void TrainingDataCollectorImpl::OnGetSegmentInfoAtDecisionTime(
    proto::SegmentId segment_id,
    TrainingRequestId request_id,
    DecisionType type,
    scoped_refptr<InputContext> input_context,
    const proto::SegmentInfo& segment_info,
    std::optional<ModelProvider::Request> inputs) {
  TrainingTimings training_request = ComputeDecisionTiming(segment_info);
  if (!CanReportTrainingData(segment_info, /*include_outputs*/ false)) {
    return;
  }

  if (type != segment_info.model_metadata()
                  .training_outputs()
                  .trigger_config()
                  .decision_type()) {
    RecordTrainingDataCollectionEvent(
        segment_id,
        stats::TrainingDataCollectionEvent::kOnDecisionTimeTypeMistmatch);
    return;
  }

  auto collection_event =
      stats::TrainingDataCollectionEvent::kImmediateCollectionStart;
  if (IsPeriodic(segment_info)) {
    collection_event =
        stats::TrainingDataCollectionEvent::kContinousCollectionStart;
    if (NeedsExactPredictionTime(segment_info)) {
      collection_event = collection_event = stats::TrainingDataCollectionEvent::
          kContinousExactPredictionTimeCollectionStart;
    }
  }
  RecordTrainingDataCollectionEvent(segment_info.segment_id(),
                                    collection_event);

  if (inputs) {
    OnGetTrainingTensorsAtDecisionTime(request_id, training_request,
                                       segment_info, /*has_error=*/false,
                                       *inputs, {});
    return;
  }

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
  if (has_error) {
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kGetInputTensorsFailed);
    return;
  } else {
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kCollectAndStoreInputsSuccess);
  }

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
                               segment_info.model_source(),
                               std::move(training_data),
                               /*save_to_db=*/store_to_disk);

  // Set up delayed output recordings based on time delay triggers defined
  // in model metadata.
  // TODO(haileywang): This is slightly inaccurate since the the delay timer is
  // only started after the input training tensors are cached.
  if (training_request.observation_delayed_task) {
    VLOG(1) << "Observation timeout set for "
            << proto::SegmentId_Name(segment_info.segment_id()) << " "
            << *training_request.observation_delayed_task;

    if (training_request.observation_delayed_task.value().is_zero()) {
      PostObservationTask(
          request_id, segment_info, *training_request.observation_delayed_task,
          stats::TrainingDataCollectionEvent::kImmediateObservationPosted);
    } else {
      // Sample time triggered data for ondemand models.
      if (IsPeriodic(segment_info) ||
          base::RandGenerator(time_trigger_sampling_rate_) == 0) {
        PostObservationTask(
            request_id, segment_info,
            *training_request.observation_delayed_task,
            stats::TrainingDataCollectionEvent::kDelayedTaskPosted);
        VLOG(1) << "Delayed task posted for " << segment_info.segment_id();
      } else {
        RecordTrainingDataCollectionEvent(
            segment_info.segment_id(),
            stats::TrainingDataCollectionEvent::kDelayTriggerSampled);
      }
    }
  } else {
    RecordTrainingDataCollectionEvent(
        segment_info.segment_id(),
        stats::TrainingDataCollectionEvent::kWaitingForNonDelayedTrigger);
  }
}

void TrainingDataCollectorImpl::PostObservationTask(
    TrainingRequestId request_id,
    const proto::SegmentInfo& segment_info,
    const base::TimeDelta& delay,
    stats::TrainingDataCollectionEvent event) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TrainingDataCollectorImpl::OnObservationTrigger,
                     weak_ptr_factory_.GetWeakPtr(), std::nullopt, request_id,
                     segment_info, base::DoNothing()),
      delay);
  RecordTrainingDataCollectionEvent(segment_info.segment_id(), event);
}

void TrainingDataCollectorImpl::OnObservationTrigger(
    const std::optional<ImmediateCollectionParam>& param,
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
      segment_info.segment_id(), segment_info.model_source(), request_id,
      base::BindOnce(&TrainingDataCollectorImpl::OnGetStoredTrainingData,
                     weak_ptr_factory_.GetWeakPtr(), param, segment_info,
                     std::move(callback)));
}

void TrainingDataCollectorImpl::OnGetStoredTrainingData(
    const std::optional<ImmediateCollectionParam>& param,
    const proto::SegmentInfo& segment_info,
    SuccessCallback callback,
    std::optional<proto::TrainingData> input) {
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

  VLOG(1) << "Generating training data for segment "
          << proto::SegmentId_Name(segment_info.segment_id())
          << ". Prediction time: " << prediction_time
          << " Observation time: " << observation_time;

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
    const std::optional<ImmediateCollectionParam>& param,
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
  base::Time current_time = clock_->Now();

  // Check for delay triggers in the config.
  std::optional<uint64_t> delay_sec;
  for (int i = 0; i < training_config.observation_trigger_size(); i++) {
    const auto& trigger = training_config.observation_trigger(i);
    if (trigger.delay_sec() > 0) {
      delay_sec = trigger.delay_sec();
    }
  }

  bool exact_prediction_time = training_config.use_exact_prediction_time();

  if (IsPeriodic(info)) {
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
      training_request.observation_delayed_task = std::nullopt;
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
  bool flexible_observation_period =
      training_config.use_flexible_observation_time();

  // Check for delay triggers in the config.
  std::optional<uint64_t> delay_sec;
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
  if (IsPeriodic(info) && !delay_sec) {
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

  // Only periodic segments need storage to disk and can be multi-session.
  // If the exact prediction time is not used, we could recompute inputs at
  // observation time so we don't need to store to disk.
  // If the observation is triggered immediately, we don't need to store to
  // disk.
  // If delay is not specified, then the collector cannot know when to trigger
  // observation and the training data will live in database forever. So, it is
  // safe to verify the delay before storing to disk.
  bool store_to_disk = IsPeriodic(segment_info) &&
                       NeedsExactPredictionTime(segment_info) &&
                       training_request.observation_delayed_task.has_value();

  return store_to_disk;
}

}  // namespace segmentation_platform
