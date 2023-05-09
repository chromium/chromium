// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/request_dispatcher.h"
#include <set>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/config_holder.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/selection/request_handler.h"
#include "components/segmentation_platform/internal/selection/segment_result_provider.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

namespace {

// Amount of time to wait for model initialization. During this period requests
// for uninitialized models will be enqueued and processed either when the model
// is ready or when this timeout expires. Time is 200ms to cover 80% of cases
// (According to
// OptimizationGuide.ModelHandler.HandlerCreatedToModelAvailable histogram).
const int kModelInitializationTimeoutMs = 200;

// Wrap callback to record metrics.
template <typename ResultType>
base::OnceCallback<void(const ResultType&)> GetWrappedCallback(
    const std::string& segmentation_key,
    base::OnceCallback<void(const ResultType&)> callback) {
  auto wrapped_callback = base::BindOnce(
      [](const std::string& segmentation_key, base::Time start_time,
         base::OnceCallback<void(const ResultType&)> callback,
         const ResultType& result) -> void {
        stats::RecordClassificationRequestTotalDuration(
            segmentation_key, base::Time::Now() - start_time);
        std::move(callback).Run(result);
      },
      segmentation_key, base::Time::Now(), std::move(callback));

  return wrapped_callback;
}
}  // namespace

RequestDispatcher::RequestDispatcher(
    const ConfigHolder* config_holder,
    CachedResultProvider* cached_result_provider)
    : config_holder_(config_holder),
      cached_result_provider_(cached_result_provider) {
  std::set<proto::SegmentId> found_segments;

  // Individual models must be loaded from disk or fetched from network. Fill a
  // list to keep track of which ones are still pending.
  uninitialized_segmentation_keys_ =
      config_holder_->non_legacy_segmentation_keys();
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
    for (const auto& config : config_holder_->configs()) {
      request_handlers_[config->segmentation_key] = RequestHandler::Create(
          *config, std::move(result_providers[config->segmentation_key]),
          execution_service);
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
  auto key_for_updated_segment = config_holder_->GetKeyForSegmentId(segment_id);
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

template <typename ResultType, typename Request>
void RequestDispatcher::GetModelResult(
    const std::string& segmentation_key,
    const PredictionOptions& options,
    scoped_refptr<InputContext> input_context,
    Request request,
    base::OnceCallback<void(const ResultType&)> callback) {
  if (config_holder_->IsLegacySegmentationKey(segmentation_key)) {
    return;
  }

  // TODO(ssid): Support cached results for all APIs.
  DCHECK(options.on_demand_execution);

  // For on-demand results, we need to run the models for which we need DB
  // initialization to be complete. Hence cache the request if platform
  // initialization isn't completed yet.
  if (!storage_init_status_.has_value() ||
      uninitialized_segmentation_keys_.contains(segmentation_key)) {
    // If the platform isn't fully initialized, cache the input arguments to
    // run later.
    pending_actions_[segmentation_key].push_back(base::BindOnce(
        &RequestDispatcher::GetModelResult<ResultType, Request>,
        weak_ptr_factory_.GetWeakPtr(), segmentation_key, options,
        std::move(input_context), std::move(request), std::move(callback)));
    return;
  }

  // If the platform initialization failed, invoke callback to return invalid
  // results.
  if (!storage_init_status_.value()) {
    stats::RecordSegmentSelectionFailure(
        segmentation_key,
        stats::SegmentationSelectionFailureReason::kDBInitFailure);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  ResultType(PredictionStatus::kFailed)));
    return;
  }

  auto wrapped_callback =
      GetWrappedCallback(segmentation_key, std::move(callback));

  auto iter = request_handlers_.find(segmentation_key);
  CHECK(iter != request_handlers_.end());
  std::move(request).Run(iter->second.get(), options, input_context,
                         std::move(wrapped_callback));
}

void RequestDispatcher::GetClassificationResult(
    const std::string& segmentation_key,
    const PredictionOptions& options,
    scoped_refptr<InputContext> input_context,
    ClassificationResultCallback callback) {
  if (!options.on_demand_execution) {
    auto wrapped_callback =
        GetWrappedCallback(segmentation_key, std::move(callback));

    // Returns result directly from prefs for non-ondemand models.
    auto result =
        cached_result_provider_->GetCachedResultForClient(segmentation_key);

    stats::RecordSegmentSelectionFailure(
        segmentation_key, result.status == PredictionStatus::kSucceeded
                              ? stats::SegmentationSelectionFailureReason::
                                    kClassificationResultFromPrefs
                              : stats::SegmentationSelectionFailureReason::
                                    kClassificationResultNotAvailableInPrefs);

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(wrapped_callback), result));
    return;
  }

  GetModelResult(segmentation_key, options, input_context,
                 base::BindOnce(&RequestHandler::GetClassificationResult),
                 std::move(callback));
}

void RequestDispatcher::GetAnnotatedNumericResult(
    const std::string& segmentation_key,
    const PredictionOptions& options,
    scoped_refptr<InputContext> input_context,
    AnnotatedNumericResultCallback callback) {
  GetModelResult(segmentation_key, options, input_context,
                 base::BindOnce(&RequestHandler::GetAnnotatedNumericResult),
                 std::move(callback));
}

int RequestDispatcher::GetPendingActionCountForTesting() {
  int total_actions = 0;
  for (auto& actions_for_key : pending_actions_) {
    total_actions += actions_for_key.second.size();
  }
  return total_actions;
}

}  // namespace segmentation_platform
