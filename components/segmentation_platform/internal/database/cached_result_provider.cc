// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/cached_result_provider.h"

#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "components/segmentation_platform/internal/logging.h"
#include "components/segmentation_platform/internal/post_processor/post_processor.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/proto/prediction_result.pb.h"
#include "components/segmentation_platform/public/result.h"

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
  for (const auto& config : *configs_) {
    absl::optional<proto::ClientResult> client_result =
        result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);
    bool has_valid_result = client_result.has_value() &&
                            client_result->client_result().result_size() > 0 &&
                            client_result->client_result().has_output_config();
    stats::RecordSegmentSelectionFailure(
        *config, has_valid_result ? stats::SegmentationSelectionFailureReason::
                                        kSelectionAvailableInProtoPrefs
                                  : stats::SegmentationSelectionFailureReason::
                                        kInvalidSelectionResultInProtoPrefs);
    if (has_valid_result) {
      client_result_from_last_session_map_.insert(
          {config->segmentation_key, client_result->client_result()});
    } else {
      client_result_from_last_session_map_.erase(config->segmentation_key);
    }
  }
}

CachedResultProvider::~CachedResultProvider() = default;

ClassificationResult CachedResultProvider::GetCachedResultForClient(
    const std::string& segmentation_key) {
  PostProcessor post_processor;
  auto prediction_result = GetPredictionResultForClient(segmentation_key);

  if (!prediction_result.has_value()) {
    return ClassificationResult(PredictionStatus::kFailed);
  }

  bool has_valid_result = prediction_result->result_size() > 0 &&
                          prediction_result->has_output_config();
  PredictionStatus status = has_valid_result ? PredictionStatus::kSucceeded
                                             : PredictionStatus::kFailed;

  auto post_processed_result =
      post_processor.GetPostProcessedClassificationResult(
          prediction_result.value(), status);

  return post_processed_result;
}

absl::optional<proto::PredictionResult>
CachedResultProvider::GetPredictionResultForClient(
    const std::string& segmentation_key) {
  const auto iter = client_result_from_last_session_map_.find(segmentation_key);
  if (iter == client_result_from_last_session_map_.end()) {
    return absl::nullopt;
  }

  VLOG(1) << "CachedResultProvider loaded prefs with results from previous "
             "session: "
          << segmentation_platform::PredictionResultToDebugString(iter->second)
          << " for segmentation key " << segmentation_key;
  return iter->second;
}

}  // namespace segmentation_platform
