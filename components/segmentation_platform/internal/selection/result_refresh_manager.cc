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
}  // namespace

ResultRefreshManager::ResultRefreshManager(
    const std::vector<std::unique_ptr<Config>>& configs,
    std::unique_ptr<CachedResultWriter> cached_result_writer,
    const PlatformOptions& platform_options)
    : configs_(configs),
      cached_result_writer_(std::move(cached_result_writer)),
      platform_options_(platform_options) {}

ResultRefreshManager::~ResultRefreshManager() = default;

void ResultRefreshManager::RefreshModelResults(
    std::map<std::string, std::unique_ptr<SegmentResultProvider>>
        result_providers) {
  result_providers_ = std::move(result_providers);
  for (const auto& config : configs_) {
    if (config->on_demand_execution ||
        !metadata_utils::HasMigratedToMultiOutput(config.get())) {
      continue;
    }
    auto* segment_result_provider =
        result_providers_[config->segmentation_key].get();
    GetCachedResultOrRunModel(segment_result_provider, config.get());
  }
}

void ResultRefreshManager::GetCachedResultOrRunModel(
    SegmentResultProvider* segment_result_provider,
    Config* config) {
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

void ResultRefreshManager::OnGetCachedResultOrRunModel(
    SegmentResultProvider* segment_result_provider,
    Config* config,
    std::unique_ptr<SegmentResultProvider::SegmentResult> result) {
  SegmentResultProvider::ResultState result_state =
      result ? result->state : SegmentResultProvider::ResultState::kUnknown;

  if (!SupportMultiOutput(result.get())) {
    return;
  }

  proto::PredictionResult pred_result = result->result;
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

  if (unexpired_score_from_db || expired_score_and_run_model) {
    proto::ClientResult client_result =
        metadata_utils::CreateClientResultFromPredResult(pred_result,
                                                         base::Time::Now());
    cached_result_writer_->UpdatePrefsIfExpired(config, client_result,
                                                platform_options_);
  }
}

}  // namespace segmentation_platform