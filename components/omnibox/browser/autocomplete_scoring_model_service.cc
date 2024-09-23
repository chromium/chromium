// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_scoring_model_service.h"

#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/common/trace_event_common.h"
#include "components/omnibox/browser/autocomplete_scoring_model_executor.h"
#include "components/omnibox/browser/autocomplete_scoring_model_handler.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/autocomplete_scoring_model_metadata.pb.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace {

const char kAutocompleteScoringModelMetadataTypeUrl[] =
    "type.googleapis.com/"
    "google.internal.chrome.optimizationguide.v1."
    "AutocompleteScoringModelMetadata";

// The current version the client supports for the autocomplete scoring model.
// This should be incremented any time we update the client code to add new
// scoring signals beyond those which are currently supported for ML scoring.
extern const int32_t kAutocompleteScoringModelVersion = 1;

void LogMLScoreCacheHit(bool cache_hit) {
  base::UmaHistogramBoolean(
      "Omnibox.URLScoringModelExecuted.MLScoreCache.CacheHit", cache_hit);
}

}  // namespace

AutocompleteScoringModelService::AutocompleteScoringModelService(
    optimization_guide::OptimizationGuideModelProvider* model_provider)
    : score_cache_(OmniboxFieldTrial::GetMLConfig().max_ml_score_cache_size) {
  // `model_provider` may be null for tests.
  if (OmniboxFieldTrial::IsUrlScoringModelEnabled() && model_provider) {
    model_executor_task_runner_ =
        base::SequencedTaskRunner::GetCurrentDefault();

    optimization_guide::proto::Any any_metadata;
    any_metadata.set_type_url(kAutocompleteScoringModelMetadataTypeUrl);
    optimization_guide::proto::AutocompleteScoringModelMetadata model_metadata;
    model_metadata.set_version(kAutocompleteScoringModelVersion);
    model_metadata.SerializeToString(any_metadata.mutable_value());

    url_scoring_model_handler_ =
        std::make_unique<AutocompleteScoringModelHandler>(
            model_provider, model_executor_task_runner_.get(),
            std::make_unique<AutocompleteScoringModelExecutor>(),
            optimization_guide::proto::OPTIMIZATION_TARGET_OMNIBOX_URL_SCORING,
            /*model_metadata=*/any_metadata);
  }
}

AutocompleteScoringModelService::~AutocompleteScoringModelService() = default;

void AutocompleteScoringModelService::AddOnModelUpdatedCallback(
    base::OnceClosure callback) {
  url_scoring_model_handler_->AddOnModelUpdatedCallback(std::move(callback));
}

int AutocompleteScoringModelService::GetModelVersion() const {
  auto info = url_scoring_model_handler_->GetModelInfo();
  return info.has_value() ? info->GetVersion() : -1;
}

std::vector<AutocompleteScoringModelService::Result>
AutocompleteScoringModelService::BatchScoreAutocompleteUrlMatchesSync(
    const std::vector<const ScoringSignals*>& batch_scoring_signals) {
  TRACE_EVENT0(
      "omnibox",
      "AutocompleteScoringModelService::BatchScoreAutocompleteUrlMatchesSync");
  if (!UrlScoringModelAvailable()) {
    return {};
  }

  std::optional<std::vector<std::vector<float>>> batch_model_input =
      url_scoring_model_handler_->GetBatchModelInput(batch_scoring_signals);
  if (!batch_model_input) {
    return {};
  }

  std::vector<Result> batch_results(batch_model_input->size());

  if (OmniboxFieldTrial::GetMLConfig().ml_url_score_caching) {
    std::vector<size_t> uncached_positions;
    std::vector<std::vector<float>> uncached_inputs;

    // Source ML scores from the in-memory cache when possible.
    for (size_t i = 0; i < batch_model_input->size(); ++i) {
      const auto model_input = batch_model_input->at(i);
      const auto it = score_cache_.Get(model_input);

      const bool cache_hit = it != score_cache_.end();
      if (cache_hit) {
        batch_results[i] = it->second;
      } else {
        uncached_positions.push_back(i);
        uncached_inputs.push_back(model_input);
      }
      LogMLScoreCacheHit(cache_hit);
    }

    // Synchronous model execution.
    const auto batch_model_output =
        url_scoring_model_handler_->BatchExecuteModelWithInputSync(
            uncached_inputs);

    size_t i = 0;
    for (const auto& model_output : batch_model_output) {
      batch_results[uncached_positions[i]] =
          model_output ? std::make_optional(model_output->at(0)) : std::nullopt;
      if (model_output) {
        score_cache_.Put(uncached_inputs.at(i), model_output->at(0));
      }
      ++i;
    }
  } else {
    // Synchronous model execution.
    const auto batch_model_output =
        url_scoring_model_handler_->BatchExecuteModelWithInputSync(
            *batch_model_input);
    size_t i = 0;
    for (const auto& model_output : batch_model_output) {
      batch_results[i++] =
          model_output ? std::make_optional(model_output->at(0)) : std::nullopt;
    }
  }

  return batch_results;
}

bool AutocompleteScoringModelService::UrlScoringModelAvailable() {
  return url_scoring_model_handler_ &&
         url_scoring_model_handler_->ModelAvailable();
}
