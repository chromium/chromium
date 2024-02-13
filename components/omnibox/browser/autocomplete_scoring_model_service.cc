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
#include "components/optimization_guide/proto/models.pb.h"

AutocompleteScoringModelService::AutocompleteScoringModelService(
    optimization_guide::OptimizationGuideModelProvider* model_provider) {
  // `model_provider` may be null for tests.
  if (OmniboxFieldTrial::IsUrlScoringModelEnabled() && model_provider) {
    model_executor_task_runner_ =
        base::SequencedTaskRunner::GetCurrentDefault();
    url_scoring_model_handler_ =
        std::make_unique<AutocompleteScoringModelHandler>(
            model_provider, model_executor_task_runner_.get(),
            std::make_unique<AutocompleteScoringModelExecutor>(),
            optimization_guide::proto::OPTIMIZATION_TARGET_OMNIBOX_URL_SCORING,
            /*model_metadata=*/std::nullopt);
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

  // Synchronous model execution.
  const auto batch_model_output =
      url_scoring_model_handler_->BatchExecuteModelWithInputSync(
          *batch_model_input);
  std::vector<Result> batch_results;
  batch_results.reserve(batch_model_output.size());
  for (const auto& model_output : batch_model_output) {
    batch_results.emplace_back(
        model_output ? std::make_optional(model_output->at(0)) : std::nullopt);
  }
  return batch_results;
}

bool AutocompleteScoringModelService::UrlScoringModelAvailable() {
  return url_scoring_model_handler_ &&
         url_scoring_model_handler_->ModelAvailable();
}
