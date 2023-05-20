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
    const ScoringSignals& scoring_signals,
    const std::string& stripped_destination_url,
    ResultCallback result_callback) {
  TRACE_EVENT0("omnibox",
               "AutocompleteScoringModelService::ScoreAutocompleteUrlMatch");

  if (!UrlScoringModelAvailable()) {
    std::move(result_callback)
        .Run(std::make_tuple(absl::nullopt, stripped_destination_url));
    return;
  }

  absl::optional<std::vector<float>> input_signals =
      url_scoring_model_handler_->GetModelInput(scoring_signals);
  if (!input_signals) {
    std::move(result_callback)
        .Run(std::make_tuple(absl::nullopt, stripped_destination_url));
    return;
  }

  url_scoring_model_handler_->ExecuteModelWithInput(
      tracker,
      base::BindOnce(&AutocompleteScoringModelService::ProcessModelOutput,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result_callback),
                     stripped_destination_url),
      *input_signals);
}

bool AutocompleteScoringModelService::UrlScoringModelAvailable() {
  return url_scoring_model_handler_ &&
         url_scoring_model_handler_->ModelAvailable();
}

void AutocompleteScoringModelService::BatchScoreAutocompleteUrlMatches(
    base::CancelableTaskTracker* tracker,
    const std::vector<const ScoringSignals*>& batch_scoring_signals,
    const std::vector<std::string>& stripped_destination_urls,
    BatchResultCallback batch_result_callback) {
  TRACE_EVENT0(
      "omnibox",
      "AutocompleteScoringModelService::BatchScoreAutocompleteUrlMatches");

  // Function for creating a result vector with null scores.
  auto create_null_results = [&]() {
    std::vector<Result> results;
    for (size_t i = 0; i < batch_scoring_signals.size(); i++) {
      results.emplace_back(absl::nullopt, stripped_destination_urls.at(i));
    }
    return results;
  };

  if (!UrlScoringModelAvailable()) {
    std::move(batch_result_callback).Run(create_null_results());
    return;
  }

  absl::optional<std::vector<std::vector<float>>> batch_input =
      url_scoring_model_handler_->GetBatchModelInput(batch_scoring_signals);
  if (!batch_input) {
    std::move(batch_result_callback).Run(create_null_results());
    return;
  }

  url_scoring_model_handler_->BatchExecuteModelWithInput(
      tracker,
      base::BindOnce(&AutocompleteScoringModelService::ProcessBatchModelOutput,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(batch_result_callback),
                     stripped_destination_urls),
      *batch_input);
}

void AutocompleteScoringModelService::ProcessModelOutput(
    ResultCallback result_callback,
    const std::string& stripped_destination_url,
    const absl::optional<AutocompleteScoringModelExecutor::ModelOutput>&
        model_output) {
  TRACE_EVENT0("omnibox",
               "AutocompleteScoringModelService::ProcessModelOutput");
  if (model_output.has_value()) {
    if (!model_output.value().empty()) {
      std::move(result_callback)
          .Run(std::make_tuple(model_output.value()[0],
                               stripped_destination_url));
      return;
    }
    NOTREACHED() << "The model generated an empty output vector.";
  }
  std::move(result_callback)
      .Run(std::make_tuple(absl::nullopt, stripped_destination_url));
}

void AutocompleteScoringModelService::ProcessBatchModelOutput(
    BatchResultCallback batch_result_callback,
    const std::vector<std::string>& stripped_destination_urls,
    const std::vector<absl::optional<ModelOutput>>& batch_model_output) {
  TRACE_EVENT0("omnibox",
               "AutocompleteScoringModelService::ProcessBatchModelOutput");

  std::vector<Result> batch_results;
  for (size_t i = 0; i < stripped_destination_urls.size(); i++) {
    const auto& output = batch_model_output.at(i);
    batch_results.emplace_back(
        output ? absl::optional<float>(output->at(0)) : absl::nullopt,
        stripped_destination_urls.at(i));
  }

  std::move(batch_result_callback).Run(std::move(batch_results));
}
