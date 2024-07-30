// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/cached_result_writer.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/logging.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/post_processor/post_processor.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"

namespace segmentation_platform {

CachedResultWriter::CachedResultWriter(ClientResultPrefs* prefs,
                                       base::Clock* clock)
    : result_prefs_(prefs), clock_(clock) {}

CachedResultWriter::~CachedResultWriter() = default;

bool CachedResultWriter::UpdatePrefsIfExpired(
    const Config* config,
    proto::ClientResult client_result,
    const PlatformOptions& platform_options) {
  if (!IsPrefUpdateRequiredForClient(config, client_result, platform_options)) {
    return false;
  }
  VLOG(1) << "CachedResultWriter updating prefs with new result: "
          << segmentation_platform::PredictionResultToDebugString(
                 client_result.client_result())
          << " for segmentation key: " << config->segmentation_key;
  UpdateNewClientResultToPrefs(config, std::move(client_result));
  return true;
}

void CachedResultWriter::MarkResultAsUsed(const Config* config) {
  const proto::ClientResult* old_result =
      result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);
  if (!old_result || old_result->first_used_timestamp() > 0) {
    return;
  }
  proto::ClientResult new_result = *old_result;

  new_result.set_first_used_timestamp(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  result_prefs_->SaveClientResultToPrefs(config->segmentation_key,
                                         std::move(new_result));
}

void CachedResultWriter::CacheModelExecution(
    const Config* config,
    const proto::PredictionResult& result) {
  auto now = base::Time::Now();
  proto::ClientResult update =
      metadata_utils::CreateClientResultFromPredResult(result, now);
  update.set_first_used_timestamp(
      now.ToDeltaSinceWindowsEpoch().InMicroseconds());
  result_prefs_->SaveClientResultToPrefs(config->segmentation_key, update);
}

bool CachedResultWriter::IsPrefUpdateRequiredForClient(
    const Config* config,
    const proto::ClientResult& new_client_result,
    const PlatformOptions& platform_options) {
  const proto::ClientResult* old_client_result =
      result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);
  if (!old_client_result) {
    return true;
  }
  const proto::PredictionResult& old_pred_result =
      old_client_result->client_result();
  const proto::PredictionResult& new_pred_result =
      new_client_result.client_result();

  // When migrating from legacy, `model_version` would be missing. We consider
  // model is updated, if old model version is missing or if new model version
  // is greater than old model version. We also assume model_version is always
  // increasing monotonically.
  bool is_model_updated =
      new_pred_result.has_model_version() &&
      (!old_pred_result.has_model_version() ||
       (old_pred_result.has_model_version() &&
        old_pred_result.model_version() < new_pred_result.model_version()));

  if (is_model_updated &&
      new_pred_result.output_config().ignore_previous_model_ttl()) {
    return true;
  }
  PostProcessor post_processor;
  base::TimeDelta ttl_to_use =
      post_processor.GetTTLForPredictedResult(old_pred_result);
  base::Time expiration_time =
      base::Time::FromDeltaSinceWindowsEpoch(
          base::Microseconds(old_client_result->timestamp_us())) +
      ttl_to_use;

  bool force_refresh_results = platform_options.force_refresh_results;
  bool has_expired_results = expiration_time <= clock_->Now();

  if (!force_refresh_results && !has_expired_results) {
    stats::RecordSegmentSelectionFailure(
        *config, stats::SegmentationSelectionFailureReason::
                     kProtoPrefsUpdateNotRequired);
    VLOG(1) << __func__ << ": previous client_result for segmentation_key: "
            << config->segmentation_key
            << " has not yet expired. Expiration: " << expiration_time;
    return false;
  }
  return true;
}

void CachedResultWriter::UpdateNewClientResultToPrefs(
    const Config* config,
    proto::ClientResult client_result) {
  const proto::ClientResult* prev_client_result =
      result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);
  const proto::PredictionResult* prev_prediction_result =
      prev_client_result ? &prev_client_result->client_result() : nullptr;
  if (prev_prediction_result &&
      PostProcessor::IsClassificationResult(*prev_prediction_result) &&
      PostProcessor::IsClassificationResult(client_result.client_result())) {
    stats::RecordClassificationResultUpdated(*config, prev_prediction_result,
                                             client_result.client_result());
  }
  stats::RecordSegmentSelectionFailure(
      *config, stats::SegmentationSelectionFailureReason::kProtoPrefsUpdated);
  result_prefs_->SaveClientResultToPrefs(config->segmentation_key,
                                         std::move(client_result));
}

}  // namespace segmentation_platform
