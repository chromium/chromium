// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/cached_result_provider.h"

#include "base/task/single_thread_task_runner.h"
#include "components/segmentation_platform/internal/post_processor/post_processor.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"

namespace segmentation_platform {

CachedResultProvider::CachedResultProvider(
    PrefService* pref_service,
    const std::vector<std::unique_ptr<Config>>& configs)
    : CachedResultProvider(std::make_unique<ClientResultPrefs>(pref_service),
                           configs) {}

CachedResultProvider::CachedResultProvider(
    std::unique_ptr<ClientResultPrefs> prefs,
    const std::vector<std::unique_ptr<Config>>& configs)
    : configs_(configs), result_prefs_(std::move(prefs)) {
  PostProcessor post_processor;

  for (const auto& config : *configs_) {
    absl::optional<proto::ClientResult> client_result =
        result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);
    bool has_valid_result = client_result.has_value() &&
                            client_result->client_result().result_size() > 0 &&
                            client_result->client_result().has_output_config();
    proto::PredictionResult pred_result = has_valid_result
                                              ? client_result->client_result()
                                              : proto::PredictionResult();
    PredictionStatus status = has_valid_result ? PredictionStatus::kSucceeded
                                               : PredictionStatus::kFailed;
    // TODO(ritikagup): Revisit if the metrics need to be stored here.
    stats::RecordSegmentSelectionFailure(
        *config, has_valid_result ? stats::SegmentationSelectionFailureReason::
                                        kSelectionAvailableInPrefs
                                  : stats::SegmentationSelectionFailureReason::
                                        kInvalidSelectionResultInPrefs);

    auto post_processed_result =
        post_processor.GetPostProcessedClassificationResult(pred_result,
                                                            status);
    client_result_from_last_session_map_.insert(
        {config->segmentation_key, post_processed_result});
  }
}

CachedResultProvider::~CachedResultProvider() = default;

ClassificationResult CachedResultProvider::GetCachedResultForClient(
    const std::string& segmentation_key) {
  const auto iter = client_result_from_last_session_map_.find(segmentation_key);
  return iter == client_result_from_last_session_map_.end()
             ? ClassificationResult(PredictionStatus::kFailed)
             : iter->second;
}

}  // namespace segmentation_platform