// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/request_handler.h"

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/segmentation_platform/internal/post_processor/post_processor.h"
#include "components/segmentation_platform/internal/selection/segment_result_provider.h"
#include "components/segmentation_platform/internal/selection/selection_utils.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/trigger.h"

namespace segmentation_platform {
namespace {

class RequestHandlerImpl : public RequestHandler {
 public:
  RequestHandlerImpl(const Config& config,
                     std::unique_ptr<SegmentResultProvider> result_provider,
                     ExecutionService* execution_service,
                     StorageService* storage_service);
  ~RequestHandlerImpl() override;

  // Disallow copy/assign.
  RequestHandlerImpl(const RequestHandlerImpl&) = delete;
  RequestHandlerImpl& operator=(const RequestHandlerImpl&) = delete;

  // RequestHandler impl.
  void GetPredictionResult(const PredictionOptions& options,
                           scoped_refptr<InputContext> input_context,
                           RawResultCallback callback) override;

 private:
  void GetModelResult(const PredictionOptions& options,
                      scoped_refptr<InputContext> input_context,
                      SegmentResultProvider::SegmentResultCallback callback);

  void OnGetPredictionResult(
      const PredictionOptions& options,
      scoped_refptr<InputContext> input_context,
      RawResultCallback callback,
      std::unique_ptr<SegmentResultProvider::SegmentResult> result);

  TrainingRequestId CollectTrainingData(
      scoped_refptr<InputContext> input_context,
      ModelProvider::Request inputs);

  // The config for providing client config params.
  const raw_ref<const Config> config_;

  // The result provider responsible for getting the result, either by running
  // the model or getting results from the cache as necessary.
  std::unique_ptr<SegmentResultProvider> result_provider_;

  // Pointer to the execution service.
  const raw_ptr<ExecutionService> execution_service_ = nullptr;

  const raw_ptr<StorageService> storage_service_ = nullptr;

  base::WeakPtrFactory<RequestHandlerImpl> weak_ptr_factory_{this};
};

RequestHandlerImpl::RequestHandlerImpl(
    const Config& config,
    std::unique_ptr<SegmentResultProvider> result_provider,
    ExecutionService* execution_service,
    StorageService* storage_service)
    : config_(config),
      result_provider_(std::move(result_provider)),
      execution_service_(execution_service),
      storage_service_(storage_service) {}

RequestHandlerImpl::~RequestHandlerImpl() = default;

void RequestHandlerImpl::GetPredictionResult(
    const PredictionOptions& options,
    scoped_refptr<InputContext> input_context,
    RawResultCallback callback) {
  DCHECK(options.on_demand_execution ||
         (!options.on_demand_execution && options.fallback_allowed));
  GetModelResult(options, input_context,
                 base::BindOnce(&RequestHandlerImpl::OnGetPredictionResult,
                                weak_ptr_factory_.GetWeakPtr(), options,
                                input_context, std::move(callback)));
}

void RequestHandlerImpl::GetModelResult(
    const PredictionOptions& options,
    scoped_refptr<InputContext> input_context,
    SegmentResultProvider::SegmentResultCallback callback) {
  DCHECK_EQ(config_->segments.size(), 1u);
  auto result_options =
      std::make_unique<SegmentResultProvider::GetResultOptions>();

  // Note that, this assumes that a client has only one model.
  result_options->segment_id = config_->segments.begin()->first;
  result_options->ignore_db_scores = true;
  result_options->input_context = input_context;
  result_options->callback = std::move(callback);

  result_provider_->GetSegmentResult(std::move(result_options));
}

void RequestHandlerImpl::OnGetPredictionResult(
    const PredictionOptions& options,
    scoped_refptr<InputContext> input_context,
    RawResultCallback callback,
    std::unique_ptr<SegmentResultProvider::SegmentResult> segment_result) {
  RawResult result(PredictionStatus::kFailed);
  if (segment_result) {
    auto status =
        selection_utils::ResultStateToPredictionStatus(segment_result->state);
    result = PostProcessor().GetRawResult(segment_result->result, status);
    if (status == PredictionStatus::kSucceeded) {
      CHECK(segment_result->model_inputs)
          << "Handler should be used only for on demand execution: "
          << config_->segmentation_key;
      result.request_id = CollectTrainingData(
          input_context, std::move(*segment_result->model_inputs));
    }

    stats::RecordSegmentSelectionFailure(
        *config_, stats::GetSuccessOrFailureReason(segment_result->state));
    stats::RecordClassificationResultComputed(*config_, segment_result->result);
    // Update prefs for future requests.
    if (options.can_update_cache_for_future_requests) {
      proto::ClientResult client_result =
          metadata_utils::CreateClientResultFromPredResult(
              segment_result->result, base::Time::Now());
      storage_service_->cached_result_writer()->UpdatePrefsIfExpired(
          &(*config_), client_result, PlatformOptions::CreateDefault());
    }
  }
  std::move(callback).Run(std::move(result));
}

TrainingRequestId RequestHandlerImpl::CollectTrainingData(
    scoped_refptr<InputContext> input_context,
    ModelProvider::Request inputs) {
  // The training data collector might be null in testing.
  if (!execution_service_->training_data_collector()) {
    return TrainingRequestId();
  }

  return execution_service_->training_data_collector()->OnDecisionTime(
      config_->segments.begin()->first, input_context,
      proto::TrainingOutputs::TriggerConfig::ONDEMAND, std::move(inputs));
}

}  // namespace

// static
std::unique_ptr<RequestHandler> RequestHandler::Create(
    const Config& config,
    std::unique_ptr<SegmentResultProvider> result_provider,
    ExecutionService* execution_service,
    StorageService* storage_service) {
  return std::make_unique<RequestHandlerImpl>(
      config, std::move(result_provider), execution_service, storage_service);
}

}  // namespace segmentation_platform
