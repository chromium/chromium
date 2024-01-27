// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/result_refresh_manager.h"

#include "base/metrics/field_trial_params.h"
#include "components/segmentation_platform/internal/selection/selection_utils.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"

namespace segmentation_platform {

namespace {

const int kModelInitializationTimeoutMs = 5000;

int GetModelInitializationTimeoutMs() {
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kSegmentationPlatformModelInitializationDelay,
      kModelInitializationDelay, kModelInitializationTimeoutMs);
}

// Checks if the model result supports multi output model.
bool SupportMultiOutput(SegmentResultProvider::SegmentResult* result) {
  return result && result->result.has_output_config();
}

// Collects training data.
void CollectTrainingDataIfNeeded(const Config* config,
                                 ExecutionService* execution_service) {
  // The execution service and training data collector might be null in testing.
  if (execution_service && execution_service->training_data_collector()) {
    for (const auto& segment : config->segments) {
      execution_service->training_data_collector()->OnDecisionTime(
          segment.first, nullptr,
          proto::TrainingOutputs::TriggerConfig::PERIODIC, std::nullopt,
          /*decision_result_update_trigger=*/true);
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

void ResultRefreshManager::Initialize(
    std::map<std::string, std::unique_ptr<SegmentResultProvider>>
        result_providers,
    ExecutionService* execution_service) {
  result_providers_ = std::move(result_providers);
  execution_service_ = execution_service;

  delay_state_ = platform_options_.disable_model_execution_delay
                     ? DelayState::DELAY_EXECUTED
                     : DelayState::DELAY_NOT_HIT;
}

void ResultRefreshManager::RefreshModelResults(bool is_startup) {
  if (delay_state_ == DelayState::DELAY_NOT_HIT && is_startup) {
    // Set a delay timeout to execute all the models after the delay
    // `kModelInitializationTimeoutMs` is hit. This is to get finch seed to load
    // before model execution.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ResultRefreshManager::RefreshModelResultsInternal,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Milliseconds(GetModelInitializationTimeoutMs()));
    return;
  }
  if (delay_state_ == DelayState::DELAY_EXECUTED) {
    RefreshModelResultsInternal();
  }
}

void ResultRefreshManager::RefreshModelResultsInternal() {
  delay_state_ = DelayState::DELAY_EXECUTED;
  for (const auto& config : config_holder_->configs()) {
    GetCachedResultOrRunModel(config.get());
  }
}

void ResultRefreshManager::GetCachedResultOrRunModel(const Config* config) {
  if (!config->auto_execute_and_cache ||
      metadata_utils::ConfigUsesLegacyOutput(config)) {
    return;
  }
  auto* segment_result_provider =
      result_providers_[config->segmentation_key].get();
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

  result_options->callback = base::BindOnce(
      &ResultRefreshManager::OnGetCachedResultOrRunModel,
      weak_ptr_factory_.GetWeakPtr(), segment_result_provider, config);

  segment_result_provider->GetSegmentResult(std::move(result_options));
}

void ResultRefreshManager::OnModelUpdated(proto::SegmentInfo* segment_info) {
  const Config* config =
      config_holder_->GetConfigForSegmentId(segment_info->segment_id());
  if (config->segmentation_key.empty() ||
      delay_state_ == DelayState::DELAY_NOT_HIT) {
    return;
  }
  GetCachedResultOrRunModel(config);
}

void ResultRefreshManager::OnGetCachedResultOrRunModel(
    SegmentResultProvider* segment_result_provider,
    const Config* config,
    std::unique_ptr<SegmentResultProvider::SegmentResult> result) {
  SegmentResultProvider::ResultState result_state = result->state;

  // If the model result is available either from database or running the
  // model, update prefs if expired.
  PredictionStatus status =
      selection_utils::ResultStateToPredictionStatus(result_state);

  if (status != PredictionStatus::kSucceeded) {
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

  // Recording this even for success case.
  stats::RecordSegmentSelectionFailure(
      *config, stats::GetSuccessOrFailureReason(result_state));

  stats::RecordClassificationResultComputed(*config, result->result);

  proto::ClientResult client_result =
      metadata_utils::CreateClientResultFromPredResult(
          std::move(result->result), base::Time::Now());
  bool is_pref_updated = cached_result_writer_->UpdatePrefsIfExpired(
      config, std::move(client_result), platform_options_);
  if (is_pref_updated) {
    CollectTrainingDataIfNeeded(config, execution_service_);
  }
}

}  // namespace segmentation_platform
