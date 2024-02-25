// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/request_dispatcher.h"

#include <set>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/config_holder.h"
#include "components/segmentation_platform/internal/post_processor/post_processor.h"
#include "components/segmentation_platform/internal/selection/request_handler.h"
#include "components/segmentation_platform/internal/selection/segment_result_provider.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/result.h"

namespace segmentation_platform {

namespace {

// Amount of time to wait for model initialization. During this period requests
// for uninitialized models will be enqueued and processed either when the model
// is ready or when this timeout expires. Time is 200ms to cover 80% of cases
// (According to
// OptimizationGuide.ModelHandler.HandlerCreatedToModelAvailable histogram).
const int kModelInitializationTimeoutMs = 200;

void PostProcess(const RawResult& raw_result, ClassificationResult& result) {
  result = PostProcessor().GetPostProcessedClassificationResult(
      std::move(raw_result.result), raw_result.status);
  result.request_id = raw_result.request_id;
}
void PostProcess(const RawResult& raw_result, AnnotatedNumericResult& result) {
  result = raw_result;
}

}  // namespace

RequestDispatcher::RequestDispatcher(StorageService* storage_service)
    : storage_service_(storage_service) {
  std::set<proto::SegmentId> found_segments;

  // Individual models must be loaded from disk or fetched from network. Fill a
  // list to keep track of which ones are still pending.
  uninitialized_segmentation_keys_ =
      storage_service_->config_holder()->non_legacy_segmentation_keys();
}

RequestDispatcher::~RequestDispatcher() = default;

void RequestDispatcher::OnPlatformInitialized(
    bool success,
    ExecutionService* execution_service,
    std::map<std::string, std::unique_ptr<SegmentResultProvider>>
        result_providers) {
  storage_init_status_ = success;

  // Only set request handlers if it has not been set for testing already.
  if (request_handlers_.empty()) {
    for (const auto& config : storage_service_->config_holder()->configs()) {
      request_handlers_[config->segmentation_key] = RequestHandler::Create(
          *config, std::move(result_providers[config->segmentation_key]),
          execution_service, storage_service_);
    }
  }

  // Set a timeout to execute all pending requests even if their models didn't
  // initialize after |kModelInitializationTimeoutMs|. This is to avoid waiting
  // for long periods of time when models need to be downloaded, and to avoid
  // requests waiting forever when there's no model.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RequestDispatcher::OnModelInitializationTimeout,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(kModelInitializationTimeoutMs));
}

void RequestDispatcher::ExecuteAllPendingActions() {
  while (!pending_actions_.empty()) {
    ExecutePendingActionsForKey(pending_actions_.begin()->first);
  }
}

void RequestDispatcher::ExecutePendingActionsForKey(
    const std::string& segmentation_key) {
  auto pending_actions_for_key = pending_actions_.find(segmentation_key);

  if (pending_actions_for_key == pending_actions_.end()) {
    return;
  }

  while (!pending_actions_for_key->second.empty()) {
    auto callback = std::move(pending_actions_for_key->second.front());
    pending_actions_for_key->second.pop_front();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }

  pending_actions_.erase(segmentation_key);
}

void RequestDispatcher::OnModelUpdated(proto::SegmentId segment_id) {
  auto key_for_updated_segment =
      storage_service_->config_holder()->GetKeyForSegmentId(segment_id);
  if (!key_for_updated_segment) {
    return;
  }
  const std::string& segmentation_key = *key_for_updated_segment;

  uninitialized_segmentation_keys_.erase(segmentation_key);
  ExecutePendingActionsForKey(segmentation_key);
}

void RequestDispatcher::OnModelInitializationTimeout() {
  uninitialized_segmentation_keys_.clear();
  ExecuteAllPendingActions();
}

template <typename ResultType>
void RequestDispatcher::CallbackWrapper(
    const std::string& segmentation_key,
    base::Time start_time,
    base::OnceCallback<void(const ResultType&)> callback,
    bool is_cached_result,
    const RawResult& raw_result) {
  Config* config =
      storage_service_->config_holder()->GetConfigForSegmentationKey(
          segmentation_key);
  CHECK(config);

  stats::RecordClassificationRequestTotalDuration(
      *config, base::Time::Now() - start_time);

  ResultType result(PredictionStatus::kFailed);
  PostProcess(std::move(raw_result), result);
  VLOG(1) << "Computed result for " << segmentation_key << ": "
          << result.ToDebugString();
  std::move(callback).Run(result);
}

void RequestDispatcher::GetModelResult(
    const std::string& segmentation_key,
    const PredictionOptions& options,
    scoped_refptr<InputContext> input_context,
    WrappedCallback callback) {
  if (storage_service_->config_holder()->IsLegacySegmentationKey(
          segmentation_key)) {
    LOG(ERROR)
        << "Segmentation key: " << segmentation_key
        << " is using a legacy config with the new API which is not "
           "supported. Legacy segments should migrate to the new config.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*is_cached_result=*/false,
                       RawResult(PredictionStatus::kFailed)));
    return;
  }

  Config* config =
      storage_service_->config_holder()->GetConfigForSegmentationKey(
          segmentation_key);
  CHECK(config);

  if (options.on_demand_execution) {
    ExecuteOnDemand(segmentation_key, config, options, input_context,
                    std::move(callback));
    return;
  }
  HandleCachedExecution(segmentation_key, config, options, input_context,
                        std::move(callback));
}

void RequestDispatcher::ExecuteOnDemand(
    const std::string& segmentation_key,
    const Config* config,
    const PredictionOptions& options,
    scoped_refptr<InputContext> input_context,
    WrappedCallback callback) {
  DCHECK(options.on_demand_execution ||
         (!options.on_demand_execution && options.fallback_allowed));
  // For on-demand results, we need to run the models for which we need DB
  // initialization to be complete. Hence cache the request if platform
  // initialization isn't completed yet.
  if (!storage_init_status_.has_value() ||
      uninitialized_segmentation_keys_.contains(segmentation_key)) {
    // If the platform isn't fully initialized, cache the input arguments to
    // run later.
    pending_actions_[segmentation_key].push_back(
        base::BindOnce(&RequestDispatcher::GetModelResult,
                       weak_ptr_factory_.GetWeakPtr(), segmentation_key,
                       options, std::move(input_context), std::move(callback)));
    return;
  }

  // If the platform initialization failed, invoke callback to return invalid
  // results.
  if (!storage_init_status_.value()) {
    stats::RecordSegmentSelectionFailure(
        *config, stats::SegmentationSelectionFailureReason::kDBInitFailure);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*is_cached_result=*/false,
                       RawResult(PredictionStatus::kFailed)));
    return;
  }

  auto iter = request_handlers_.find(segmentation_key);
  CHECK(iter != request_handlers_.end());
  auto final_callback =
      base::BindOnce(&RequestDispatcher::OnFinishedOnDemandExecution,
                     weak_ptr_factory_.GetWeakPtr(), segmentation_key, config,
                     options, input_context, std::move(callback));
  iter->second->GetPredictionResult(options, input_context,
                                    std::move(final_callback));
}

void RequestDispatcher::OnFinishedOnDemandExecution(
    const std::string& segmentation_key,
    const Config* config,
    const PredictionOptions& options,
    scoped_refptr<InputContext> input_context,
    WrappedCallback callback,
    const RawResult& raw_result) {
  if (raw_result.status == PredictionStatus::kFailed) {
    // If there is no result from ondemand execution and fallback is enabled
    // return cached result if previously result was cached.
    if (options.on_demand_execution && options.fallback_allowed &&
        options.can_update_cache_for_future_requests) {
      HandleCachedExecution(segmentation_key, config, options, input_context,
                            std::move(callback));
      return;
    }
    stats::RecordSegmentSelectionFailure(
        *config, stats::SegmentationSelectionFailureReason::
                     kOnDemandModelExecutionFailed);
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), /*is_cached_result=*/false,
                                std::move(raw_result)));
}

void RequestDispatcher::HandleCachedExecution(
    const std::string& segmentation_key,
    const Config* config,
    const PredictionOptions& options,
    scoped_refptr<InputContext> input_context,
    WrappedCallback callback) {
  // Returns result directly from prefs for non-ondemand models.
  auto pred_result =
      storage_service_->cached_result_provider()->GetPredictionResultForClient(
          segmentation_key);

  RawResult raw_result(PredictionStatus::kFailed);
  bool cached_execution_fallback_on_failure =
      !options.on_demand_execution && options.fallback_allowed;

  if (!pred_result && cached_execution_fallback_on_failure) {
    // Execute ondemand if no cached result is available and fallback is
    // allowed. Only supported for cached execution.
    stats::RecordSegmentSelectionFailure(
        *config, stats::SegmentationSelectionFailureReason::
                     kCachedResultUnavailableExecutingOndemand);
    ExecuteOnDemand(segmentation_key, config, options, input_context,
                    std::move(callback));
    return;
  }

  if (pred_result) {
    // Return cached result.
    raw_result = PostProcessor().GetRawResult(*pred_result,
                                              PredictionStatus::kSucceeded);

    storage_service_->cached_result_writer()->MarkResultAsUsed(config);
    stats::RecordSegmentSelectionFailure(
        *config, stats::SegmentationSelectionFailureReason::
                     kClassificationResultFromPrefs);
  } else {
    // Return failure if no cached result is available.
    // This happens in two scenarios:
    // 1. Ondemand execution fails and fallback execution also fails.
    // 2. Cached execution fails and fallback is not allowed.
    stats::RecordSegmentSelectionFailure(
        *config, stats::SegmentationSelectionFailureReason::
                     kClassificationResultNotAvailableInPrefs);
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), /*is_cached_result=*/true,
                                std::move(raw_result)));
}

void RequestDispatcher::GetClassificationResult(
    const std::string& segmentation_key,
    const PredictionOptions& options,
    scoped_refptr<InputContext> input_context,
    ClassificationResultCallback callback) {
  auto wrapped_callback =
      base::BindOnce(&RequestDispatcher::CallbackWrapper<ClassificationResult>,
                     weak_ptr_factory_.GetWeakPtr(), segmentation_key,
                     base::Time::Now(), std::move(callback));
  GetModelResult(segmentation_key, options, input_context,
                 std::move(wrapped_callback));
}

void RequestDispatcher::GetAnnotatedNumericResult(
    const std::string& segmentation_key,
    const PredictionOptions& options,
    scoped_refptr<InputContext> input_context,
    AnnotatedNumericResultCallback callback) {
  auto wrapped_callback = base::BindOnce(
      &RequestDispatcher::CallbackWrapper<AnnotatedNumericResult>,
      weak_ptr_factory_.GetWeakPtr(), segmentation_key, base::Time::Now(),
      std::move(callback));
  GetModelResult(segmentation_key, options, input_context,
                 std::move(wrapped_callback));
}

int RequestDispatcher::GetPendingActionCountForTesting() {
  int total_actions = 0;
  for (auto& actions_for_key : pending_actions_) {
    total_actions += actions_for_key.second.size();
  }
  return total_actions;
}

}  // namespace segmentation_platform
