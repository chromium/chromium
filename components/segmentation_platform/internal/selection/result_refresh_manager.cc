// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/result_refresh_manager.h"

#include "base/task/single_thread_task_runner.h"
#include "components/segmentation_platform/internal/post_processor/post_processor.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"

namespace segmentation_platform {

namespace {
// Checks if the model result supports multi output model.
bool SupportMultiOutput(SegmentResultProvider::SegmentResult* result) {
  return result && result->result.has_output_config();
}

// Collects training data after model execution.
void CollectTrainingData(const Config* config,
                         ExecutionService* execution_service) {
  // The execution service and training data collector might be null in testing.
  if (execution_service && execution_service->training_data_collector()) {
    for (const auto& segment : config->segments) {
      execution_service->training_data_collector()->OnDecisionTime(
          segment.first, nullptr,
          proto::TrainingOutputs::TriggerConfig::PERIODIC);
    }
  }
}

}  // namespace

ResultRefreshManager::ResultRefreshManager(
    const ConfigHolder* config_holder,
    CachedResultWriter* cached_result_writer,
    const PlatformOptions& platform_options)
    : config_holder_(config_holder),
      cached_result_writer_(cached_result_writer),
      platform_options_(platform_options) {}

ResultRefreshManager::~ResultRefreshManager() = default;

void ResultRefreshManager::RefreshModelResults(
    std::map<std::string, std::unique_ptr<SegmentResultProvider>>
        result_providers,
    ExecutionService* execution_service) {
  result_providers_ = std::move(result_providers);

  for (const auto& config : config_holder_->configs()) {
    if (config->on_demand_execution ||
        metadata_utils::ConfigUsesLegacyOutput(config.get())) {
      continue;
    }
    auto* segment_result_provider =
        result_providers_[config->segmentation_key].get();
    GetCachedResultOrRunModel(segment_result_provider, config.get(),
                              execution_service);
  }
}

void ResultRefreshManager::GetCachedResultOrRunModel(
    SegmentResultProvider* segment_result_provider,
    const Config* config,
    ExecutionService* execution_service) {
  auto result_options =
      std::make_unique<SegmentResultProvider::GetResultOptions>();
  // Not required, checking only for testing.
  if (config->segments.empty()) {
    return;
  }
  // Note that, this assumes that a client has only one model.
  result_options->segment_id = config->segments.begin()->first;
  result_options->ignore_db_scores = false;
  result_options->save_results_to_db = true;

  result_options->callback =
      base::BindOnce(&ResultRefreshManager::OnGetCachedResultOrRunModel,
                     weak_ptr_factory_.GetWeakPtr(), segment_result_provider,
                     config, execution_service);

  segment_result_provider->GetSegmentResult(std::move(result_options));
}

void ResultRefreshManager::OnModelUpdated(proto::SegmentInfo* segment_info,
                                          ExecutionService* execution_service) {
  const Config* config =
      config_holder_->GetConfigForSegmentId(segment_info->segment_id());
  if (config->segmentation_key.empty()) {
    return;
  }
  GetCachedResultOrRunModel(result_providers_[config->segmentation_key].get(),
                            config, execution_service);
}

void ResultRefreshManager::OnGetCachedResultOrRunModel(
    SegmentResultProvider* segment_result_provider,
    const Config* config,
    ExecutionService* execution_service,
    std::unique_ptr<SegmentResultProvider::SegmentResult> result) {
  SegmentResultProvider::ResultState result_state = result->state;

  // If the model result is available either from database or running the
  // model, update prefs if expired.
  bool unexpired_score_from_db =
      (result_state ==
       SegmentResultProvider::ResultState::kSuccessFromDatabase);
  bool expired_score_and_run_model =
      ((result_state ==
        SegmentResultProvider::ResultState::kTfliteModelScoreUsed) ||
       (result_state ==
        SegmentResultProvider::ResultState::kDefaultModelScoreUsed));

  bool success = (unexpired_score_from_db || expired_score_and_run_model);

  if (!success) {
    stats::RecordSegmentSelectionFailure(
        *config, stats::GetSuccessOrFailureReason(result_state));
    return;
  }

  if (!SupportMultiOutput(result.get())) {
    stats::RecordSegmentSelectionFailure(
        *config,
        stats::SegmentationSelectionFailureReason::kMultiOutputNotSupported);
    return;
  }

  proto::PredictionResult pred_result = result->result;
  stats::RecordClassificationResultComputed(*config, pred_result);

  proto::ClientResult client_result =
      metadata_utils::CreateClientResultFromPredResult(pred_result,
                                                       base::Time::Now());
  cached_result_writer_->UpdatePrefsIfExpired(config, client_result,
                                              platform_options_);

  CollectTrainingData(config, execution_service);
}

}  // namespace segmentation_platform