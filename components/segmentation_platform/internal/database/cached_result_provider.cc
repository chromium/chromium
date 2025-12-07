// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/cached_result_provider.h"

#include "base/logging.h"
#include "components/segmentation_platform/internal/logging.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/proto/prediction_result.pb.h"

namespace segmentation_platform {

CachedResultProvider::CachedResultProvider(
    ClientResultPrefs* prefs,
    const std::vector<std::unique_ptr<Config>>& configs)
    : configs_(configs), result_prefs_(prefs) {
  for (const auto& config : *configs_) {
    const proto::ClientResult* client_result =
        result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);
    bool has_valid_result = client_result &&
                            client_result->client_result().result_size() > 0 &&
                            client_result->client_result().has_output_config();
    has_valid_result = has_valid_result &&
                       metadata_utils::ValidateOutputConfig(
                           client_result->client_result().output_config()) ==
                           metadata_utils::ValidationResult::kValidationSuccess;
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

std::optional<proto::PredictionResult>
CachedResultProvider::GetPredictionResultForClient(
    const std::string& segmentation_key) {
  const auto iter = client_result_from_last_session_map_.find(segmentation_key);
  if (iter == client_result_from_last_session_map_.end()) {
    return std::nullopt;
  }

  VLOG(1) << "CachedResultProvider loaded prefs with results from previous "
             "session: "
          << segmentation_platform::PredictionResultToDebugString(iter->second)
          << " for segmentation key " << segmentation_key;
  return iter->second;
}

}  // namespace segmentation_platform
