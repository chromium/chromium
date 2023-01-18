// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/request_dispatcher.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/segmentation_platform/internal/selection/request_handler.h"
#include "components/segmentation_platform/internal/selection/segment_result_provider.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/prediction_options.h"

namespace segmentation_platform {

RequestDispatcher::RequestDispatcher(
    const std::vector<std::unique_ptr<Config>>& configs,
    CachedResultProvider* cached_result_provider)
    : configs_(configs), cached_result_provider_(cached_result_provider) {}

RequestDispatcher::~RequestDispatcher() = default;

void RequestDispatcher::OnPlatformInitialized(
    bool success,
    ExecutionService* execution_service,
    std::map<std::string, std::unique_ptr<SegmentResultProvider>>
        result_providers) {
  storage_init_status_ = success;

  // Only set request handlers if it has not been set for testing already.
  if (request_handlers_.empty()) {
    for (const auto& config : configs_) {
      request_handlers_[config->segmentation_key] = RequestHandler::Create(
          *config, std::move(result_providers[config->segmentation_key]),
          execution_service);
    }
  }

  // Run any method calls that were received during initialization.
  while (!pending_actions_.empty()) {
    auto callback = std::move(pending_actions_.front());
    pending_actions_.pop_front();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }
}

void RequestDispatcher::GetClassificationResult(
    const std::string& segmentation_key,
    const PredictionOptions& options,
    scoped_refptr<InputContext> input_context,
    ClassificationResultCallback callback) {
  if (!options.on_demand_execution) {
    // Returns result directly from prefs for non-ondemand models.
    auto result =
        cached_result_provider_->GetCachedResultForClient(segmentation_key);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result));
    return;
  }

  // For on-demand results, we need to run the models for which we need DB
  // initialization to be complete. Hence cache the request if platform
  // initialization isn't completed yet.
  if (!storage_init_status_.has_value()) {
    // If the platform isn't fully initialized, cache the input arguments to
    // run later.
    pending_actions_.push_back(
        base::BindOnce(&RequestDispatcher::GetClassificationResult,
                       weak_ptr_factory_.GetWeakPtr(), segmentation_key,
                       options, std::move(input_context), std::move(callback)));
    return;
  }

  // If the platform initialization failed, invoke callback to return invalid
  // results.
  if (!storage_init_status_.value()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       ClassificationResult(PredictionStatus::kFailed)));
    return;
  }

  auto iter = request_handlers_.find(segmentation_key);
  CHECK(iter != request_handlers_.end());
  iter->second->GetClassificationResult(options, input_context,
                                        std::move(callback));
}

}  // namespace segmentation_platform
