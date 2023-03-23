// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_scoring_model_service.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/omnibox/browser/autocomplete_scoring_model_executor.h"
#include "components/omnibox/browser/autocomplete_scoring_model_handler.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

AutocompleteScoringModelService::AutocompleteScoringModelService(
    optimization_guide::OptimizationGuideModelProvider* model_provider) {
  model_executor_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  if (OmniboxFieldTrial::IsUrlScoringModelEnabled()) {
    // Create a URL scoring model handler.
    url_scoring_model_handler_ =
        std::make_unique<AutocompleteScoringModelHandler>(
            model_provider, model_executor_task_runner_.get(),
            std::make_unique<AutocompleteScoringModelExecutor>(),
            optimization_guide::proto::OPTIMIZATION_TARGET_OMNIBOX_URL_SCORING,
            /*model_metadata=*/absl::nullopt);
  }
}

AutocompleteScoringModelService::~AutocompleteScoringModelService() = default;

void AutocompleteScoringModelService::ScoreAutocompleteUrlMatch(
    base::CancelableTaskTracker* tracker,
    const metrics::OmniboxEventProto::Suggestion::ScoringSignals&
        scoring_signals,
    ResultCallback result_callback) {
  if (!UrlScoringModelAvailable()) {
    std::move(result_callback).Run(absl::nullopt);
    return;
  }

  absl::optional<std::vector<float>> input_signals =
      url_scoring_model_handler_->GetModelInput(scoring_signals);
  if (!input_signals) {
    std::move(result_callback).Run(absl::nullopt);
    return;
  }

  url_scoring_model_handler_->ExecuteModelWithInput(
      tracker,
      base::BindOnce(&AutocompleteScoringModelService::ProcessModelOutput,
                     base::Unretained(this), std::move(result_callback)),
      *input_signals);
}

bool AutocompleteScoringModelService::UrlScoringModelAvailable() {
  return url_scoring_model_handler_ &&
         url_scoring_model_handler_->ModelAvailable();
}

void AutocompleteScoringModelService::ProcessModelOutput(
    ResultCallback result_callback,
    const absl::optional<AutocompleteScoringModelExecutor::ModelOutput>&
        model_output) {
  if (model_output.has_value()) {
    if (!model_output.value().empty()) {
      std::move(result_callback).Run(model_output.value()[0]);
      return;
    }
    NOTREACHED() << "The model generated an empty output vector.";
  }
  std::move(result_callback).Run(absl::nullopt);
}
